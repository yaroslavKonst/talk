#ifndef _EVENT_DISPATCHER_H
#define _EVENT_DISPATCHER_H

#include <poll.h>
#include <signal.h>
#include <cstdint>

#include "Tree.hpp"

class DescriptorEventProcessor
{
public:
	virtual ~DescriptorEventProcessor()
	{ }

	virtual int GetDescriptor() = 0;
	virtual bool RequestRead() = 0;
	virtual bool RequestWrite() = 0;
	virtual void ProcessRead() = 0;
	virtual void ProcessWrite() = 0;

private:
};

class QuantEventProcessor
{
public:
	virtual ~QuantEventProcessor()
	{ }

	virtual void ProcessQuant() = 0;

private:
};

class TimeEventProcessor
{
public:
	virtual ~TimeEventProcessor()
	{ }

	virtual void ProcessTimeEvent() = 0;

	void SetTimestamp(int64_t timestamp)
	{
		_timestamp = timestamp;
	}

	int64_t GetTimestamp()
	{
		return _timestamp;
	}

	void SetInterval(int64_t interval)
	{
		_interval = interval;
	}

	int64_t GetInterval()
	{
		return _interval;
	}

private:
	int64_t _timestamp;
	int64_t _interval;
};

class SignalEventProcessor
{
public:
	virtual ~SignalEventProcessor()
	{ }

	virtual void ProcessSignal(int signum) = 0;
};

class EventDispatcher
{
public:
	EventDispatcher(
		int64_t idleInterval,
		const sigset_t *processedSignals = nullptr);
	~EventDispatcher();

	EventDispatcher(const EventDispatcher &dispatcher) = delete;
	EventDispatcher &operator=(const EventDispatcher &dispatcher) = delete;

	void Run();
	void Stop();

	void RegisterDescriptorProcessor(DescriptorEventProcessor *processor);
	void UnregisterDescriptorProcessor(DescriptorEventProcessor *processor);

	void RegisterQuantProcessor(QuantEventProcessor *processor);
	void UnregisterQuantProcessor(QuantEventProcessor *processor);

	void RegisterTimeProcessor(TimeEventProcessor *processor);
	void UnregisterTimeProcessor(TimeEventProcessor *processor);

	void RegisterSignalProcessor(
		SignalEventProcessor *processor,
		int signum);
	void UnregisterSignalProcessor(
		SignalEventProcessor *processor,
		int signum);

private:
	bool _work;

	int64_t _idleInterval;

	// Poll.
	DescriptorEventProcessor **_pollProcessors;
	struct pollfd *_pollFds;
	int _reservedFds;
	int _maxFds;

	void PreparePollFds();
	void ProcessPollFds();

	// Quant.
	struct QuantProcessorNode
	{
		QuantProcessorNode *Next;
		QuantEventProcessor *Processor;
	};

	QuantProcessorNode *_quantProcessorFirst;
	QuantProcessorNode *_quantProcessorLast;

	void ProcessQuants();

	// Time.
	struct TimeProcessorNode
	{
		TimeProcessorNode *Next;
		TimeEventProcessor *Processor;
	};

	TimeProcessorNode *_timeProcessors;

	void ProcessTime();

	// Signal.
	struct SignalProcessorNode
	{
		SignalProcessorNode *Next;
		SignalEventProcessor *Processor;
	};

	struct SignalProcessorNodeTreeEntry
	{
		int SignalNumber;
		SignalProcessorNode *Processors;

		SignalProcessorNodeTreeEntry();
		SignalProcessorNodeTreeEntry(int signum);

		bool operator==(SignalProcessorNodeTreeEntry &e) const;
		bool operator<(SignalProcessorNodeTreeEntry &e) const;
	};

	sigset_t _origSigMask;

	Tree<SignalProcessorNodeTreeEntry> _signalProcessors;

	static bool _instanceExists;

	static sigset_t _signalsToProcess;
	static volatile sig_atomic_t _hasSignals;
	static void SignalHandler(int signum);

	void SetHandler(int signum);
	void RemoveHandler(int signum);

	void ProcessSignals();
};

#endif
