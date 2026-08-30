#include "GateListeningSocket.hpp"

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "../Common/Exception.hpp"
#include "../Common/Log.hpp"
#include "../Common/File.hpp"

GateListeningSocket::GateListeningSocket(
	EventDispatcher *dispatcher,
	UserDB *users,
	Config *config,
	RateLimiter *rateLimiter)
{
	_socketFd = -1;
	_dispatcher = dispatcher;
	_users = users;
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

	int addrLen;
	struct sockaddr_storage *addr =
		_config->GetGateAddress().GetStructSockaddr(
			htons(port),
			addrLen);

	_socketFd = socket(addr->ss_family, SOCK_STREAM, 0);

	if (_socketFd == -1) {
		delete addr;
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
		delete addr;
		CloseSocket();
		THROW("Failed to set REUSEADDR on listening socket.");
	}

	res = bind(_socketFd, (struct sockaddr*)addr, addrLen);

	delete addr;

	if (res == -1) {
		CloseSocket();
		THROW("Failed to bind listening socket.");
	}

	res = listen(_socketFd, NetworkConstants::ListenBacklogSize);

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
	struct sockaddr_storage addr;
	unsigned int addrSize = sizeof(addr);

	int fd = accept(_socketFd, (struct sockaddr*)&addr, &addrSize);

	if (fd == -1) {
		return;
	}

	if (addrSize > sizeof(addr)) {
		shutdown(fd, SHUT_RDWR);
		close(fd);
		THROW("Insufficient storage space for accept.");
	}

	MakeNonblocking(fd);

	IPAddress address;

	if (!address.LoadStructSockaddr(&addr)) {
		shutdown(fd, SHUT_RDWR);
		close(fd);
		return;
	}

	InboundGateSession *session =
		new InboundGateSession(
			fd,
			address,
			this,
			_dispatcher,
			_users,
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
