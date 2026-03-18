#include "ControlSocket.hpp"

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "../Protocol/ControlParser.hpp"

ControlSocket::ControlSocket(UserDB *users, EventDispatcher *dispatcher)
{
	_socketFd = -1;
	_dispatcher = dispatcher;
	_users = users;

	_controlSessions = nullptr;
	_timeQuantRequested = false;
}

ControlSocket::~ControlSocket()
{
	if (_timeQuantRequested) {
		_dispatcher->RegisterQuantProcessor(this);
		_timeQuantRequested = false;
	}

	CloseSocket();

	while (_controlSessions) {
		ControlNode *tmp = _controlSessions;
		_controlSessions = _controlSessions->Next;
		delete tmp->Session;
		delete tmp;
	}
}

void ControlSocket::ProcessRead()
{
	int fd = accept(_socketFd, nullptr, nullptr);

	if (fd == -1) {
		return;
	}

	AddSession(fd);
}

void ControlSocket::ProcessWrite()
{
	THROW("This method must never be called.");
}

void ControlSocket::OpenSocket()
{
	_socketFd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (_socketFd == -1) {
		THROW("Failed to create control socket.");
	}

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strncpy(
		addr.sun_path,
		TALKD_SOCKET_NAME,
		sizeof(addr.sun_path) - 1);

	int res = bind(_socketFd, (struct sockaddr*)&addr, sizeof(addr));

	if (res == -1) {
		CloseSocket();
		THROW("Failed to bind control socket.");
	}

	res = listen(_socketFd, 5);

	if (res == -1) {
		CloseSocket();
		THROW("Failed to move control socket to listening state.");
	}

	_dispatcher->RegisterDescriptorProcessor(this);
}

void ControlSocket::CloseSocket()
{
	if (_socketFd != -1) {
		_dispatcher->UnregisterDescriptorProcessor(this);

		close(_socketFd);
		unlink(TALKD_SOCKET_NAME);
		_socketFd = -1;
	}
}

void ControlSocket::ProcessQuant()
{
	_timeQuantRequested = false;

	ControlNode **s = &_controlSessions;

	while (*s) {
		if ((*s)->Remove) {
			ControlNode *tmp = *s;
			*s = (*s)->Next;
			delete tmp->Session;
			delete tmp;
		} else {
			s = &(*s)->Next;
		}
	}
}

void ControlSocket::AddSession(int fd)
{
	ControlNode *s = new ControlNode;
	s->Remove = false;
	s->Next = _controlSessions;
	s->Session = new ControlSession(fd, this, _dispatcher);

	_controlSessions = s;
}

void ControlSocket::MarkSessionForRemoval(ControlSession *session)
{
	ControlNode *s = _controlSessions;

	while (s) {
		if (s->Session == session) {
			s->Remove = true;
		}

		s = s->Next;
	}

	if (!_timeQuantRequested) {
		_timeQuantRequested = true;
		_dispatcher->RegisterQuantProcessor(this);
	}
}
