#include "ServerSession.hpp"

#include <unistd.h>
#include <cstdlib>
#include <sys/socket.h>

#include "User.hpp"
#include "../Protocol/SessionParser.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/Log.hpp"
#include "../Common/Exception.hpp"

ServerSession::ServerSession(
	int fd,
	ServerSessionStorage *storage,
	Config *config,
	EventDispatcher *dispatcher,
	EncryptedStream *outES,
	EncryptedStream *inES,
	uint8_t outScramblerInit,
	uint8_t inScramblerInit)
{
	SetInterval(10);
	SetTimestamp(GetUnixTime());

	_dispatcher = dispatcher;
	_fd = fd;
	_storage = storage;
	_config = config;

	_inES = *inES;
	_outES = *outES;

	_protocol = new SessionProtocol(
		_fd,
		&_outES,
		&_inES,
		outScramblerInit,
		inScramblerInit);

	_protocol->SetInputSizeLimit(_config->GetMessageSizeLimit());

	_dispatcher->RegisterDescriptorProcessor(this);
	_dispatcher->RegisterTimeProcessor(this);
	_config->RegisterConfigUser(this);

	SessionLog("Start session.");

}

ServerSession::~ServerSession()
{
	SessionLog("End session.");

	_config->UnregisterConfigUser(this);
	_dispatcher->UnregisterDescriptorProcessor(this);
	_dispatcher->UnregisterTimeProcessor(this);

	delete _protocol;

	if (_fd != -1) {
		shutdown(_fd, SHUT_RDWR);
		close(_fd);
		_fd = -1;
	}
}

void ServerSession::ReloadConfig()
{
	_protocol->SetInputSizeLimit(_config->GetMessageSizeLimit());
	ProcessGetHostName();
}

bool ServerSession::RequestRead()
{
	return true;
}

bool ServerSession::RequestWrite()
{
	return _protocol->RequestWrite();
}

void ServerSession::ProcessRead()
{
	SetTimestamp(GetUnixTime());

	bool success = _protocol->Read();

	if (!success) {
		_storage->MarkSessionForRemoval(this);
		return;
	}

	if (!_protocol->CanReceive()) {
		return;
	}

	success = ProcessInput(_protocol->Receive());

	if (!success) {
		_storage->MarkSessionForRemoval(this);
	}
}

void ServerSession::ProcessWrite()
{
	SetTimestamp(GetUnixTime());

	bool success = _protocol->Write();

	if (!success) {
		_storage->MarkSessionForRemoval(this);
	}
}

void ServerSession::ProcessTimeEvent()
{
	_storage->MarkSessionForRemoval(this);
}

bool ServerSession::ProcessInput(const CowBuffer<uint8_t> buffer)
{
	if (buffer.Size() < sizeof(int32_t)) {
		SessionLog("Protocol violation.");
		return false;
	}

	int32_t command = *buffer.SwitchType<int32_t>();

	switch (command) {
	case SESSION_COMMAND_KEEP_ALIVE:
		return ProcessKeepAlive(buffer);
	case SESSION_COMMAND_GET_HOST_NAME:
		return ProcessGetHostName();
	default:
		SessionLog("Unknown command.");
		return false;
	}
}

bool ServerSession::ProcessKeepAlive(const CowBuffer<uint8_t> buffer)
{
	CommandKeepAlive::Command request;
	bool parseResult = CommandKeepAlive::ParseCommand(buffer, request);

	if (!parseResult) {
		return false;
	}

	_protocol->Send(buffer, 0);
	return true;
}

bool ServerSession::ProcessGetHostName()
{
	CommandGetHostName::Response response;
	response.Name = _config->GetHostName();
	CowBuffer<uint8_t> buffer = CommandGetHostName::BuildResponse(response);

	_protocol->Send(buffer, 0);
	return true;
}

void ServerSession::SessionLog(String message)
{
	Log("Session of " + _storage->GetName() + ": " + message);
}
