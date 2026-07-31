#ifndef _INBOUND_GATE_SESSION_HPP
#define _INBOUND_GATE_SESSION_HPP

#include "Config.hpp"
#include "RateLimiter.hpp"
#include "../Common/EventDispatcher.hpp"

class InboundGateSessionStorage;

class InboundGateSession :
	public DescriptorEventProcessor,
	public TimeEventProcessor
{
public:
	InboundGateSession(
		int fd,
		uint32_t ipv4,
		InboundGateSessionStorage *storage,
		EventDispatcher *dispatcher,
		RateLimiter *rateLimiter);
	~InboundGateSession();

	int GetDescriptor() override;
	bool RequestRead() override;
	bool RequestWrite() override;
	void ProcessRead() override;
	void ProcessWrite() override;

	void ProcessTimeEvent() override;

private:
	InboundGateSessionStorage *_storage;
	EventDispatcher *_dispatcher;
	RateLimiter *_rateLimiter;

	int _fd;
	uint32_t _ipv4;

	void InboundGateLog(String message);
};

class InboundGateSessionStorage
{
public:
	virtual ~InboundGateSessionStorage()
	{ }

	virtual void MarkSessionForRemoval(InboundGateSession *session) = 0;
};

#endif
