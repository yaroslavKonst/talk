#include "ListeningSocket.hpp"

#include <unistd.h>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>

#include "../Common/File.hpp"

ListeningSocket::ListeningSocket(
	UserDB *users,
	EventDispatcher *dispatcher,
	Config *config,
	FailBan *failBan)
{
	_dispatcher = dispatcher;
	_users = users;
	_config = config;
	_failBan = failBan;

	_socketFd = -1;

	_config->RegisterConfigUser(this);
}

ListeningSocket::~ListeningSocket()
{
	_config->UnregisterConfigUser(this);

	CloseSocket();
}

void ListeningSocket::ReloadConfig()
{
	CloseSocket();
	OpenSocket();
}

void ListeningSocket::ProcessRead()
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

	IPAddress address;
	bool allowed;

	if (!address.LoadStructSockaddr(&addr)) {
		allowed = false;
	} else {
		allowed = _failBan->IsAllowed(address);
	}

	if (!allowed) {
		shutdown(fd, SHUT_RDWR);
		close(fd);
		return;
	}

	MakeNonblocking(fd);

	_users->AddSession(fd, address);
}

void ListeningSocket::ProcessWrite()
{
	THROW("This method must never be called.");
}

void ListeningSocket::OpenSocket()
{
	uint16_t port = _config->GetListeningPort();

	int addrLen;
	struct sockaddr_storage *addr =
		_config->GetListeningAddress().GetStructSockaddr(
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

void ListeningSocket::CloseSocket()
{
	if (_socketFd != -1) {
		_dispatcher->UnregisterDescriptorProcessor(this);
		close(_socketFd);
		_socketFd = -1;
	}
}
