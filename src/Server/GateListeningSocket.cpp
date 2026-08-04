#include "GateListeningSocket.hpp"

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "../Common/Exception.hpp"
#include "../Common/Log.hpp"
#include "../Common/File.hpp"

GateListeningSocket::GateListeningSocket(
	EventDispatcher *dispatcher,
	Config *config,
	RateLimiter *rateLimiter)
{
	_socketFd = -1;
	_dispatcher = dispatcher;
	_config = config;
	_rateLimiter = rateLimiter;
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

	MakeNonblocking(fd);

	InboundGateSession *session =
		new InboundGateSession(
			fd,
			addr.sin_addr.s_addr,
			this,
			_dispatcher,
			_config,
			_rateLimiter);

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

void GateListeningSocket::MarkSessionForRemoval(InboundGateSession *session)
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
