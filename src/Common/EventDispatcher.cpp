#include "EventDispatcher.hpp"

#include <cstring>
#include <errno.h>

#include "UnixTime.hpp"
#include "Exception.hpp"

bool EventDispatcher::_instanceExists = false;

sigset_t EventDispatcher::_signalsToProcess;
volatile sig_atomic_t EventDispatcher::_hasSignals = false;

EventDispatcher::EventDispatcher(const sigset_t *processedSignals)
{
	if (_instanceExists) {
		THROW("Only one instance of EventDispatcher is allowed.");
	}

	_work = true;

	_pollProcessors = nullptr;
	_pollFds = nullptr;
	_reservedFds = 0;
	_maxFds = 0;

	_quantProcessorFirst = 0;
	_quantProcessorLast = 0;

	_currentTimeProcessor = nullptr;
	_removeCurrentTimeProcessor = false;

	sigemptyset(&_signalsToProcess);
	sigemptyset(&_origSigMask);

	int res = sigprocmask(SIG_BLOCK, processedSignals, &_origSigMask);

	if (res == -1) {
		THROW("Failed to set signal mask.");
	}

	_instanceExists = true;
}

EventDispatcher::~EventDispatcher()
{
	sigprocmask(SIG_SETMASK, &_origSigMask, nullptr);

	FreeFds();
	FreeQuantProcessors();
	FreeTimeProcessors();
	FreeSignalProcessors();

	_instanceExists = false;
}

void EventDispatcher::Run()
{
	_work = true;

	while (_work) {
		PreparePollFds();

		struct timespec interval;
		bool needInterval = true;

		Tree<TimeProcessorNodeTreeEntry>::Entry *entry =
			_timeProcessors.FindSmallest();

		if (_quantProcessorFirst) {
			interval.tv_sec = 0;
			interval.tv_nsec = 0;
		} else if (entry) {
			int64_t idleInterval =
				entry->Key.RunTime -
				GetMonotonicMillisecondTime();

			if (idleInterval < 0) {
				idleInterval = 0;
			}

			interval.tv_sec = idleInterval / 1000;
			interval.tv_nsec = idleInterval % 1000 * 1000000;
		} else {
			needInterval = false;
		}

		int pollRes = ppoll(
			_pollFds,
			_maxFds,
			needInterval ? &interval : nullptr,
			&_origSigMask);

		if (pollRes == -1) {
			if (errno == EINTR) {
				ProcessSignals();
				continue;
			}

			THROW(String("Error on poll: ") +
				strerror(errno) + ".");
		}

		ProcessSignals();
		ProcessPollFds();
		ProcessTime();
		ProcessQuants();
	}
}

void EventDispatcher::Stop()
{
	_work = false;
}

void EventDispatcher::RegisterDescriptorProcessor(
	DescriptorEventProcessor *processor)
{
	if (_reservedFds <= _maxFds) {
		if (!_reservedFds) {
			_reservedFds = 10;
			_pollProcessors = new DescriptorEventProcessor*[
				_reservedFds];
			_pollFds = new struct pollfd[_reservedFds];
		} else {
			DescriptorEventProcessor **oldPollProc =
				_pollProcessors;
			_pollProcessors = new DescriptorEventProcessor*[
				_reservedFds * 2];
			memcpy(
				_pollProcessors,
				oldPollProc,
				sizeof(*_pollProcessors) * _reservedFds);
			delete[] oldPollProc;

			struct pollfd *oldPollFds = _pollFds;
			_pollFds = new struct pollfd[_reservedFds * 2];
			memcpy(
				_pollFds,
				oldPollFds,
				sizeof(*_pollFds) * _reservedFds);
			delete[] oldPollFds;

			_reservedFds *= 2;
		}
	}

	_pollProcessors[_maxFds] = processor;
	memset(_pollFds + _maxFds, 0, sizeof(*_pollFds));
	++_maxFds;
}

void EventDispatcher::UnregisterDescriptorProcessor(
	DescriptorEventProcessor *processor)
{
	for (int i = 0; i < _maxFds; i++) {
		if (_pollProcessors[i] != processor) {
			continue;
		}

		if (i < _maxFds - 1) {
			_pollProcessors[i] = _pollProcessors[_maxFds - 1];
			memcpy(
				_pollFds + i,
				_pollFds + _maxFds - 1,
				sizeof(*_pollFds));
		}

		_pollProcessors[_maxFds - 1] = nullptr;
		--_maxFds;
	}
}

void EventDispatcher::RegisterQuantProcessor(
	QuantEventProcessor *processor)
{
	QuantProcessorNode *node = new QuantProcessorNode;
	node->Next = nullptr;
	node->Processor = processor;

	if (!_quantProcessorFirst) {
		_quantProcessorFirst = node;
		_quantProcessorLast = node;
	} else {
		_quantProcessorLast->Next = node;
		_quantProcessorLast = node;
	}
}

void EventDispatcher::UnregisterQuantProcessor(
	QuantEventProcessor *processor)
{
	QuantProcessorNode *prev = nullptr;
	QuantProcessorNode *curr = _quantProcessorFirst;

	while (curr) {
		if (curr->Processor == processor) {
			if (prev) {
				prev->Next = curr->Next;
			} else {
				_quantProcessorFirst = curr->Next;
			}

			if (curr == _quantProcessorLast) {
				_quantProcessorLast = prev;
			}

			QuantProcessorNode *tmp = curr;
			curr = curr->Next;
			delete tmp;
		} else {
			prev = curr;
			curr = curr->Next;
		}
	}
}

void EventDispatcher::RegisterTimeProcessor(
	TimeEventProcessor *processor)
{
	int64_t runTime = processor->GetTimestamp() + processor->GetInterval();

	Tree<TimeProcessorNodeTreeEntry>::Entry *entry =
		_timeProcessors.FindEntry(runTime);

	if (!entry) {
		_timeProcessors.AddEntry(runTime);
		entry = _timeProcessors.FindEntry(runTime);
	}

	TimeProcessorNode *node = new TimeProcessorNode;
	node->Processor = processor;
	node->Next = entry->Key.Processors;
	entry->Key.Processors = node;
}

void EventDispatcher::UnregisterTimeProcessor(
	TimeEventProcessor *processor)
{
	Tree<TimeProcessorNodeTreeEntry>::Entry *entry =
		_timeProcessors.FindSmallest();

	while (entry) {
		TimeProcessorNode **curr = &entry->Key.Processors;

		while (*curr) {
			if ((*curr)->Processor == processor) {
				TimeProcessorNode *tmp = *curr;
				*curr = (*curr)->Next;
				delete tmp;
			} else {
				curr = &(*curr)->Next;
			}
		}

		Tree<TimeProcessorNodeTreeEntry>::Entry *tmp = entry;
		entry = _timeProcessors.Next(entry);

		if (!tmp->Key.Processors) {
			_timeProcessors.RemoveEntry(tmp);
		}
	}

	if (processor == _currentTimeProcessor) {
		_removeCurrentTimeProcessor = true;
	}
}

void EventDispatcher::RegisterSignalProcessor(
	SignalEventProcessor *processor,
	int signum)
{
	Tree<SignalProcessorNodeTreeEntry>::Entry *entry =
		_signalProcessors.FindEntry(signum);

	if (!entry) {
		_signalProcessors.AddEntry(signum);
		entry = _signalProcessors.FindEntry(signum);
		SetHandler(signum);
	}

	SignalProcessorNode *node = new SignalProcessorNode;
	node->Processor = processor;
	node->Next = entry->Key.Processors;

	entry->Key.Processors = node;
}

void EventDispatcher::UnregisterSignalProcessor(
	SignalEventProcessor *processor,
	int signum)
{
	Tree<SignalProcessorNodeTreeEntry>::Entry *entry =
		_signalProcessors.FindEntry(signum);

	if (!entry) {
		return;
	}

	SignalProcessorNode **node = &entry->Key.Processors;

	while (*node) {
		if ((*node)->Processor == processor) {
			SignalProcessorNode *tmp = *node;
			*node = (*node)->Next;
			delete tmp;
		} else {
			node = &(*node)->Next;
		}
	}

	if (!entry->Key.Processors) {
		RemoveHandler(entry->Key.SignalNumber);
		_signalProcessors.RemoveEntry(entry);
	}
}

void EventDispatcher::FreeFds()
{
	if (_reservedFds) {
		delete[] _pollProcessors;
		delete[] _pollFds;
	}
}

void EventDispatcher::FreeQuantProcessors()
{
	while (_quantProcessorFirst) {
		QuantProcessorNode *tmp = _quantProcessorFirst;
		_quantProcessorFirst = _quantProcessorFirst->Next;
		delete tmp;
	}
}

void EventDispatcher::FreeTimeProcessors()
{
	Tree<TimeProcessorNodeTreeEntry>::Entry *entry =
		_timeProcessors.FindSmallest();

	while (entry) {
		while (entry->Key.Processors) {
			TimeProcessorNode *tmp = entry->Key.Processors;
			entry->Key.Processors = entry->Key.Processors->Next;
			delete tmp;
		}

		Tree<TimeProcessorNodeTreeEntry>::Entry *tmp = entry;
		entry = _timeProcessors.Next(entry);
		_timeProcessors.RemoveEntry(tmp);
	}
}

void EventDispatcher::FreeSignalProcessors()
{
	Tree<SignalProcessorNodeTreeEntry>::Entry *entry =
		_signalProcessors.FindSmallest();

	while (entry) {
		while (entry->Key.Processors) {
			SignalProcessorNode *tmp = entry->Key.Processors;
			entry->Key.Processors = entry->Key.Processors->Next;
			delete tmp;
		}

		Tree<SignalProcessorNodeTreeEntry>::Entry *tmp = entry;
		entry = _signalProcessors.Next(entry);
		RemoveHandler(tmp->Key.SignalNumber);
		_signalProcessors.RemoveEntry(tmp);
	}
}

void EventDispatcher::PreparePollFds()
{
	for (int i = 0; i < _maxFds; i++) {
		_pollFds[i].fd = _pollProcessors[i]->GetDescriptor();

		short events = 0;

		if (_pollProcessors[i]->RequestRead()) {
			events |= POLLIN;
		}

		if (_pollProcessors[i]->RequestWrite()) {
			events |= POLLOUT;
		}

		_pollFds[i].events = events;
		_pollFds[i].revents = 0;
	}
}

void EventDispatcher::ProcessPollFds()
{
	for (int i = 0; i < _maxFds; i++) {
		DescriptorEventProcessor *proc = _pollProcessors[i];

		bool canRead =
			(_pollFds[i].revents & POLLIN) ||
			(_pollFds[i].revents & POLLNVAL) ||
			(_pollFds[i].revents & POLLERR) ||
			(_pollFds[i].revents & POLLHUP);

		if (!proc->RequestRead()) {
			canRead = false;
		}

		if (canRead) {
			proc->ProcessRead();

			if (_pollProcessors[i] != proc) {
				--i;
				continue;
			}
		}

		bool canWrite = _pollFds[i].revents & POLLOUT;

		if (!proc->RequestWrite()) {
			canWrite = false;
		}

		if (canWrite) {
			proc->ProcessWrite();

			if (_pollProcessors[i] != proc) {
				--i;
				continue;
			}
		}
	}
}

void EventDispatcher::ProcessQuants()
{
	if (!_quantProcessorFirst) {
		return;
	}

	QuantProcessorNode *node = _quantProcessorFirst;

	_quantProcessorFirst = _quantProcessorFirst->Next;

	if (!_quantProcessorFirst) {
		_quantProcessorLast = nullptr;
	}

	node->Processor->ProcessQuant();
	delete node;
}

EventDispatcher::TimeProcessorNodeTreeEntry::TimeProcessorNodeTreeEntry()
{
	RunTime = 0;
	Processors = nullptr;
}

EventDispatcher::TimeProcessorNodeTreeEntry::TimeProcessorNodeTreeEntry(
	int64_t runTime)
{
	RunTime = runTime;
	Processors = nullptr;
}

bool EventDispatcher::TimeProcessorNodeTreeEntry::operator==(
	const TimeProcessorNodeTreeEntry &e) const
{
	return RunTime == e.RunTime;
}

bool EventDispatcher::TimeProcessorNodeTreeEntry::operator<(
	const TimeProcessorNodeTreeEntry &e) const
{
	return RunTime < e.RunTime;
}

void EventDispatcher::ProcessTime()
{
	int64_t currentTime = GetMonotonicMillisecondTime();

	Tree<TimeProcessorNodeTreeEntry>::Entry *entry =
		_timeProcessors.FindSmallest();

	if (!entry) {
		return;
	}

	if (entry->Key.RunTime > currentTime) {
		return;
	}

	TimeProcessorNode *node = entry->Key.Processors;

	_timeProcessors.RemoveEntry(entry);

	while (node) {
		TimeEventProcessor *proc = node->Processor;

		TimeProcessorNode *tmp = node;
		node = node->Next;
		delete tmp;

		_currentTimeProcessor = proc;
		_removeCurrentTimeProcessor = false;

		bool update =
			currentTime - proc->GetTimestamp() >=
			proc->GetInterval();

		if (update) {
			proc->SetTimestamp(currentTime);
			proc->ProcessTimeEvent();
		}

		_currentTimeProcessor = nullptr;

		if (!_removeCurrentTimeProcessor) {
			RegisterTimeProcessor(proc);
		}

		_removeCurrentTimeProcessor = false;
	}
}

EventDispatcher::SignalProcessorNodeTreeEntry::SignalProcessorNodeTreeEntry()
{
	SignalNumber = 0;
	Processors = nullptr;
}

EventDispatcher::SignalProcessorNodeTreeEntry::SignalProcessorNodeTreeEntry(
	int signum)
{
	SignalNumber = signum;
	Processors = nullptr;
}

bool EventDispatcher::SignalProcessorNodeTreeEntry::operator==(
	const SignalProcessorNodeTreeEntry &e) const
{
	return SignalNumber == e.SignalNumber;
}

bool EventDispatcher::SignalProcessorNodeTreeEntry::operator<(
	const SignalProcessorNodeTreeEntry &e) const
{
	return SignalNumber < e.SignalNumber;
}

void EventDispatcher::SignalHandler(int signum)
{
	sigaddset(&_signalsToProcess, signum);
	_hasSignals = true;
}

void EventDispatcher::SetHandler(int signum)
{
	struct sigaction action;
	memset(&action, 0, sizeof(action));

	action.sa_handler = SignalHandler;

	int res = sigaction(signum, &action, nullptr);

	if (res == -1) {
		THROW("Failed to set signal handler.");
	}
}

void EventDispatcher::RemoveHandler(int signum)
{
	struct sigaction action;
	memset(&action, 0, sizeof(action));

	action.sa_handler = SIG_DFL;

	int res = sigaction(signum, &action, nullptr);

	if (res == -1) {
		THROW("Failed to set default signal action.");
	}
}

void EventDispatcher::ProcessSignals()
{
	if (!_hasSignals) {
		return;
	}

	_hasSignals = false;

	Tree<SignalProcessorNodeTreeEntry>::Entry *entry =
		_signalProcessors.FindSmallest();

	while (entry) {
		int hasSignal = sigismember(
			&_signalsToProcess,
			entry->Key.SignalNumber);

		if (hasSignal == -1) {
			THROW("Signal set check error.");
		}

		if (hasSignal != 1) {
			entry = _signalProcessors.Next(entry);
			continue;
		}

		sigdelset(&_signalsToProcess, entry->Key.SignalNumber);
		SignalProcessorNode *node = entry->Key.Processors;
		int signum = entry->Key.SignalNumber;

		entry = _signalProcessors.Next(entry);

		while (node) {
			SignalProcessorNode *n = node;
			node = node->Next;
			n->Processor->ProcessSignal(signum);
		}

	}
}
