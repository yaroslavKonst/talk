#include "InboundGateSession.hpp"

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "../Common/Exception.hpp"
#include "../Common/File.hpp"
#include "../Common/Log.hpp"

InboundGateSession::InboundGateSession(
	int fd,
	uint32_t ipv4,
	InboundGateSessionStorage *storage,
	EventDispatcher *dispatcher,
	RateLimiter *rateLimiter)
{
	SetInterval(60);
	SetTimestamp(GetUnixTime());

	_fd = fd;
	_ipv4 = ipv4;
	_storage = storage;
	_dispatcher = dispatcher;
	_rateLimiter = rateLimiter;

	_dispatcher->RegisterTimeProcessor(this);

	InboundGateLog("Session opened.");
}

InboundGateSession::~InboundGateSession()
{
	InboundGateLog("Session closed.");

	_dispatcher->UnregisterTimeProcessor(this);

	shutdown(_fd, SHUT_RDWR);
	close(_fd);
	_fd = -1;
}

int InboundGateSession::GetDescriptor()
{
	return _fd;
}

bool InboundGateSession::RequestRead()
{
	THROW("Not implemented.");
}

bool InboundGateSession::RequestWrite()
{
	THROW("Not implemented.");
}

void InboundGateSession::ProcessRead()
{
	THROW("Not implemented.");
}

void InboundGateSession::ProcessWrite()
{
	THROW("Not implemented.");
}

void InboundGateSession::ProcessTimeEvent()
{
	InboundGateLog("Timeout.");
	_storage->MarkSessionForRemoval(this);
}

void InboundGateSession::InboundGateLog(String message)
{
	Log("Inbound gate from " + IPToString(_ipv4), message);
}
