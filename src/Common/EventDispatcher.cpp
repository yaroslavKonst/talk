#include "EventDispatcher.hpp"

#include <cstring>
#include <errno.h>

#include "UnixTime.hpp"
#include "Exception.hpp"

EventDispatcher::EventDispatcher(int64_t idleInterval)
{
	_idleInterval = idleInterval;
	_work = true;

	_pollProcessors = nullptr;
	_pollFds = nullptr;
	_reservedFds = 0;
	_maxFds = 0;

	_quantProcessorFirst = 0;
	_quantProcessorLast = 0;

	_timeProcessors = nullptr;
}

EventDispatcher::~EventDispatcher()
{
	if (_reservedFds) {
		delete[] _pollProcessors;
		delete[] _pollFds;
	}

	while (_quantProcessorFirst) {
		QuantProcessorNode *tmp = _quantProcessorFirst;
		_quantProcessorFirst = _quantProcessorFirst->Next;
		delete tmp;
	}

	while (_timeProcessors) {
		TimeProcessorNode *tmp = _timeProcessors;
		_timeProcessors = _timeProcessors->Next;
		delete tmp;
	}
}

void EventDispatcher::Run()
{
	_work = true;

	while (_work) {
		PreparePollFds();

		int64_t interval = _quantProcessorFirst ? 0 : _idleInterval;

		int pollRes = poll(_pollFds, _maxFds, interval);

		if (pollRes == -1) {
			if (errno == EINTR) {
				continue;
			}

			THROW("Error on poll.");
		}

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

			delete curr;
		} else {
			prev = curr;
			curr = curr->Next;
		}
	}
}

void EventDispatcher::RegisterTimeProcessor(
	TimeEventProcessor *processor)
{
	TimeProcessorNode *node = new TimeProcessorNode;
	node->Processor = processor;
	node->Next = _timeProcessors;
	_timeProcessors = node;
}

void EventDispatcher::UnregisterTimeProcessor(
	TimeEventProcessor *processor)
{
	TimeProcessorNode **curr = &_timeProcessors;

	while (*curr) {
		if ((*curr)->Processor == processor) {
			TimeProcessorNode *tmp = *curr;
			*curr = (*curr)->Next;
			delete tmp;
		} else {
			curr = &(*curr)->Next;
		}
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

		if (canRead) {
			_pollProcessors[i]->ProcessRead();

			if (_pollProcessors[i] != proc) {
				--i;
				continue;
			}
		}

		bool canWrite = _pollFds[i].revents & POLLOUT;

		if (canWrite) {
			_pollProcessors[i]->ProcessWrite();

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

void EventDispatcher::ProcessTime()
{
	TimeProcessorNode *node = _timeProcessors;

	int64_t currentTime = GetUnixTime();

	while (node) {
		bool update =
			currentTime - node->Processor->GetTimestamp() >=
			node->Processor->GetInterval();

		if (update) {
			node->Processor->SetTimestamp(currentTime);
			node->Processor->ProcessTimeEvent();
		}

		node = node->Next;
	}
}
