#ifndef _EVENT_DISPATCHER_H
#define _EVENT_DISPATCHER_H

#include <poll.h>
#include <cstdint>

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

class EventDispatcher
{
public:
	EventDispatcher(int64_t idleInterval);
	~EventDispatcher();

	void Run();
	void Stop();

	void RegisterDescriptorProcessor(DescriptorEventProcessor *processor);
	void UnregisterDescriptorProcessor(DescriptorEventProcessor *processor);

	void RegisterQuantProcessor(QuantEventProcessor *processor);
	void UnregisterQuantProcessor(QuantEventProcessor *processor);

	void RegisterTimeProcessor(TimeEventProcessor *processor);
	void UnregisterTimeProcessor(TimeEventProcessor *processor);

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
};

#endif
