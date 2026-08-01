#include "OutboundGateSession.hpp"

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "../Message/Message.hpp"
#include "../Common/Exception.hpp"
#include "../Common/File.hpp"
#include "../Common/Log.hpp"

String OutboundGateSession::TaskProcessChannel::GetConnectionDestination()
{
	return Destination;
}

OutboundGateSession::OutboundGateSession(
	EventDispatcher *dispatcher,
	OutboundGateSessionStorage *storage,
	TaskBase *task) :
	_resolver(dispatcher)
{
	SetInterval(60);
	SetTimestamp(GetUnixTime());

	_fd = -1;
	_ipv4 = 0xffffffff;
	_dispatcher = dispatcher;
	_storage = storage;
	_task = task;

	if (!_task) {
		THROW("Task can not be NULL.");
	}

	_resolver.SetResolverUser(this);

	_dispatcher->RegisterTimeProcessor(this);

	StartConnection();

	OutboundGateLog("Session opened.");
}

OutboundGateSession::~OutboundGateSession()
{
	OutboundGateLog("Session closed.");

	_dispatcher->UnregisterTimeProcessor(this);

	_resolver.SetResolverUser(nullptr);

	if (_fd != -1) {
		shutdown(_fd, SHUT_RDWR);
		close(_fd);
		_fd = -1;
	}

	if (_task) {
		delete _task;
		_task = nullptr;
	}
}

int OutboundGateSession::GetDescriptor()
{
	return _fd;
}

bool OutboundGateSession::RequestRead()
{
	THROW("Not implemented.");
}

bool OutboundGateSession::RequestWrite()
{
	THROW("Not implemented.");
}

void OutboundGateSession::ProcessRead()
{
	THROW("Not implemented.");
}

void OutboundGateSession::ProcessWrite()
{
	THROW("Not implemented.");
}

void OutboundGateSession::ProcessTimeEvent()
{
	OutboundGateLog("Timeout.");
	_storage->MarkSessionForRemoval(this);
}

void OutboundGateSession::ResolveCompleted()
{
#warning TODO: run connection.
}

void OutboundGateSession::StartConnection()
{
	String fullName = _task->GetConnectionDestination();

	String hostName;
	String serviceName;

	bool parseResult = Message::ExtractServerDataFromFullName(
		fullName,
		hostName,
		serviceName);

	if (!parseResult) {
		THROW("Invalid name in server database: " + fullName + ".");
	}

	_resolver.RequestResolve(hostName, serviceName, SOCK_STREAM);

	_state = State::WaitingForDestinationNameResolve;
}

void OutboundGateSession::OutboundGateLog(String message)
{
	Log("Outbound gate to " + _task->GetConnectionDestination(), message);
}
