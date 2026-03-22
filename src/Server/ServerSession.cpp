#include "ServerSession.hpp"

#include <unistd.h>
#include <sys/socket.h>

#include "User.hpp"
#include "../Common/UnixTime.hpp"

ServerSession::ServerSession(
	int fd,
	ServerSessionStorage *storage,
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

	_inES = *inES;
	_outES = *outES;

	_protocol = new SessionProtocol(
		_fd,
		&_outES,
		&_inES,
		outScramblerInit,
		inScramblerInit);

	_dispatcher->RegisterDescriptorProcessor(this);
	_dispatcher->RegisterTimeProcessor(this);
}

ServerSession::~ServerSession()
{
	_dispatcher->UnregisterDescriptorProcessor(this);
	_dispatcher->UnregisterTimeProcessor(this);

	delete _protocol;

	if (_fd != -1) {
		shutdown(_fd, SHUT_RDWR);
		close(_fd);
		_fd = -1;
	}
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

	ProcessInput(_protocol->Receive());
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

void ServerSession::ProcessInput(CowBuffer<uint8_t> buffer)
{
}
