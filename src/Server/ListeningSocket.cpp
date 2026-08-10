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
	struct sockaddr_in addr;
	unsigned int addrSize = sizeof(addr);

	int fd = accept(_socketFd, (struct sockaddr*)&addr, &addrSize);

	if (fd == -1) {
		return;
	}

	bool allowed = _failBan->IsAllowed(addr.sin_addr.s_addr);

	if (!allowed) {
		shutdown(fd, SHUT_RDWR);
		close(fd);
		return;
	}

	MakeNonblocking(fd);

	_users->AddSession(fd, addr.sin_addr.s_addr);
}

void ListeningSocket::ProcessWrite()
{
	THROW("This method must never be called.");
}

void ListeningSocket::OpenSocket()
{
	uint16_t port = _config->GetListeningPort();

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = _config->GetListeningAddress();

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

void ListeningSocket::CloseSocket()
{
	if (_socketFd != -1) {
		_dispatcher->UnregisterDescriptorProcessor(this);
		close(_socketFd);
		_socketFd = -1;
	}
}
