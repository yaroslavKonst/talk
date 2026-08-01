#ifndef _OUTBOUND_GATE_SESSION_HPP
#define _OUTBOUND_GATE_SESSION_HPP

#include "Config.hpp"
#include "../Common/EventDispatcher.hpp"
#include "../Common/Resolver.hpp"

class OutboundGateSessionStorage;

class OutboundGateSession :
	public DescriptorEventProcessor,
	public TimeEventProcessor,
	public ResolverUser
{
public:
	enum class TaskType
	{
		ProcessChannel
	};

	struct TaskBase
	{
		TaskType Type;

		virtual ~TaskBase()
		{ }

		virtual String GetConnectionDestination() = 0;
	};

	struct TaskProcessChannel : public TaskBase
	{
		String Source;
		String Destination;

		String GetConnectionDestination() override;
	};

	OutboundGateSession(
		EventDispatcher *dispatcher,
		OutboundGateSessionStorage *storage,
		TaskBase *task);
	~OutboundGateSession();

	int GetDescriptor() override;
	bool RequestRead() override;
	bool RequestWrite() override;
	void ProcessRead() override;
	void ProcessWrite() override;

	void ProcessTimeEvent() override;

	void ResolveCompleted() override;

private:
	EventDispatcher *_dispatcher;
	OutboundGateSessionStorage *_storage;

	int _fd;
	uint32_t _ipv4;

	TaskBase *_task;

	enum class State
	{
		WaitingForDestinationNameResolve
	};

	State _state;
	Resolver _resolver;

	void StartConnection();

	void OutboundGateLog(String message);
};

class OutboundGateSessionStorage
{
public:
	virtual ~OutboundGateSessionStorage()
	{ }

	virtual void MarkSessionForRemoval(OutboundGateSession *session) = 0;
};

#endif
