#include "GateSession.hpp"

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "../Common/Exception.hpp"
#include "../Common/File.hpp"
#include "../Common/Log.hpp"

GateSession::GateSession()
{
	_fd = -1;
}

GateSession::~GateSession()
{
	if (_fd != -1) {
		shutdown(_fd, SHUT_RDWR);
		close(_fd);
		_fd = -1;
	}
}

void GateSession::GateLog(String message)
{
	Log("Gate: " + message);
}

// Inbound.
GateInboundSession::GateInboundSession(
	int fd,
	GateListeningSocket *storage,
	EventDispatcher *dispatcher)
{
	SetInterval(60);
	SetTimestamp(GetUnixTime());

	_fd = fd;
	_storage = storage;
	_dispatcher = dispatcher;

	_dispatcher->RegisterTimeProcessor(this);

	_storage->MarkSessionForRemoval(this);
	GateLog("Connection attempt occured.");
}

GateInboundSession::~GateInboundSession()
{
	GateLog("End.");

	_dispatcher->UnregisterTimeProcessor(this);
}

int GateInboundSession::GetDescriptor()
{
	return _fd;
}

bool GateInboundSession::RequestRead()
{
	THROW("Not implemented.");
}

bool GateInboundSession::RequestWrite()
{
	THROW("Not implemented.");
}

void GateInboundSession::ProcessRead()
{
	THROW("Not implemented.");
}

void GateInboundSession::ProcessWrite()
{
	THROW("Not implemented.");
}

void GateInboundSession::ProcessTimeEvent()
{
	_storage->MarkSessionForRemoval(this);
}

// Outbound.
GateOutboundSession::GateOutboundSession(
	int fd,
	GateListeningSocket *storage,
	EventDispatcher *dispatcher,
	OutboundStatusProcessor *processor)
{
	SetInterval(60);
	SetTimestamp(GetUnixTime());

	_fd = fd;
	_storage = storage;
	_dispatcher = dispatcher;
	_processor = processor;

	_dispatcher->RegisterTimeProcessor(this);

	_storage->MarkSessionForRemoval(this);
	GateLog("Outbound connection attempt occured.");
}

GateOutboundSession::~GateOutboundSession()
{
	_dispatcher->UnregisterTimeProcessor(this);

	_processor->ProcessOutboundStatus(
		this,
		OutboundStatusProcessor::Status::SessionEnd);
}

int GateOutboundSession::GetDescriptor()
{
	return _fd;
}

bool GateOutboundSession::RequestRead()
{
	THROW("Not implemented.");
}

bool GateOutboundSession::RequestWrite()
{
	THROW("Not implemented.");
}

void GateOutboundSession::ProcessRead()
{
	THROW("Not implemented.");
}

void GateOutboundSession::ProcessWrite()
{
	THROW("Not implemented.");
}

void GateOutboundSession::ProcessTimeEvent()
{
	_storage->MarkSessionForRemoval(this);
}

// Listening socket.
GateListeningSocket::GateListeningSocket(
	EventDispatcher *dispatcher,
	Config *config)
{
	_socketFd = -1;
	_dispatcher = dispatcher;
	_config = config;
	_sessions = nullptr;
	_timeQuantRequested = false;

	_config->RegisterConfigUser(this);
}

GateListeningSocket::~GateListeningSocket()
{
	_config->UnregisterConfigUser(this);

	if (_timeQuantRequested) {
		_dispatcher->UnregisterQuantProcessor(this);
		_timeQuantRequested = false;
	}

	CloseSocket();

	while (_sessions) {
		SessionNode *tmp = _sessions;
		_sessions = _sessions->Next;
		delete tmp->Session;
		delete tmp;
	}
}

void GateListeningSocket::OpenSocket()
{
	uint16_t port = _config->GetGatePort();

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = _config->GetGateAddress();

	_socketFd = socket(AF_INET, SOCK_STREAM, 0);

	if (_socketFd == -1) {
		THROW("Failed to create listening socket.");
	}

	int reuseAddr = 1;
	int res = setsockopt(
		_socketFd,
		SOL_SOCKET,
		SO_REUSEADDR,
		&reuseAddr,
		sizeof(reuseAddr));

	if (res == -1) {
		CloseSocket();
		THROW("Failed to set REUSEADDR on listening socket.");
	}

	res = bind(_socketFd, (struct sockaddr*)&addr, sizeof(addr));

	if (res == -1) {
		CloseSocket();
		THROW("Failed to bind listening socket.");
	}

	res = listen(_socketFd, 5);

	if (res == -1) {
		CloseSocket();
		THROW("Failed to move socket to listening state.");
	}

	_dispatcher->RegisterDescriptorProcessor(this);
}

void GateListeningSocket::CloseSocket()
{
	if (_socketFd != -1) {
		_dispatcher->UnregisterDescriptorProcessor(this);
		close(_socketFd);
		_socketFd = -1;
	}
}

void GateListeningSocket::ReloadConfig()
{
	CloseSocket();
	OpenSocket();
}

int GateListeningSocket::GetDescriptor()
{
	return _socketFd;
}

bool GateListeningSocket::RequestRead()
{
	return true;
}

bool GateListeningSocket::RequestWrite()
{
	return false;
}

void GateListeningSocket::ProcessRead()
{
	struct sockaddr_in addr;
	unsigned int addrSize = sizeof(addr);

	int fd = accept(_socketFd, (struct sockaddr*)&addr, &addrSize);

	if (fd == -1) {
		return;
	}

	/*bool allowed = _failBan.IsAllowed(addr.sin_addr.s_addr);

	if (!allowed) {
		shutdown(fd, SHUT_RDWR);
		close(fd);
		return;
	}*/

	MakeNonblocking(fd);

	GateInboundSession *session =
		new GateInboundSession(fd, this, _dispatcher);

	SessionNode *node = new SessionNode;
	node->Next = _sessions;
	node->Session = session;
	node->Remove = false;

	_sessions = node;
}

void GateListeningSocket::ProcessWrite()
{
	THROW("This method must not be called.");
}

void GateListeningSocket::MarkSessionForRemoval(GateSession *session)
{
	SessionNode *node = _sessions;

	while (node) {
		if (node->Session == session) {
			node->Remove = true;
		}

		node = node->Next;
	}

	if (!_timeQuantRequested) {
		_dispatcher->RegisterQuantProcessor(this);
		_timeQuantRequested = true;
	}
}

void GateListeningSocket::ProcessQuant()
{
	_timeQuantRequested = false;

	SessionNode **n = &_sessions;

	while (*n) {
		if ((*n)->Remove) {
			SessionNode *tmp = *n;
			*n = (*n)->Next;

			delete tmp->Session;
			delete tmp;
		} else {
			n = &(*n)->Next;
		}
	}
}
