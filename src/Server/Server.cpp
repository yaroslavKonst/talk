#include "Server.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

Server::Server() : _configFile("talkd.conf")
{
	_sessions = nullptr;
	_listeningSocket = -1;
	_activeUsers = 0;

	GetPassword();
}

Server::~Server()
{
	CloseSessions(_sessions);
	CloseSockets();
	WipeKeys();
}

int Server::Run()
{
	bool work = true;

	OpenListeningSockets();

	_activeUsers = 0;

	int64_t currentTime = GetUnixTime();

	while (work) {
		struct pollfd *fds = BuildPollFds();

		int res = poll(fds, _activeUsers + 1, 10000);

		if (res == -1) {
			THROW("Error on poll.");
		}

		int64_t newTime = GetUnixTime();
		bool updateTime = newTime - currentTime >= 10;

		if (updateTime) {
			currentTime = newTime;
		}

		ServerSession **session = &_sessions;
		ServerSession *sessionsToRemove = nullptr;

		for (int i = 1; i <= _activeUsers; i++)
		{
			if (fds[i].revents & POLLOUT) {
				(*session)->Write();
			}

			if (fds[i].revents & POLLIN) {
				(*session)->Read();
			}

			bool endSession = false;

			if ((*session)->Input && !(*session)->ExpectedInput) {
				endSession = !(*session)->Process();
			}

			if (!endSession && updateTime) {
				endSession = !(*session)->TimePassed();
			}

			if (endSession) {
				ServerSession *sessionToRm = *session;

				*session = (*session)->Next;

				sessionToRm->Next = sessionsToRemove;
				sessionsToRemove = sessionToRm;
			} else {
				session = &((*session)->Next);
			}
		}

		CloseSessions(sessionsToRemove);

		if (fds[0].revents & POLLIN) {
			AcceptConnection();
		}

		delete[] fds;
	}

	return 0;
}

void Server::GetPassword()
{
	// Password file.
	int passFd = open("talkd.shadow", O_RDONLY);

	if (passFd == -1) {
		passFd = 0;
		// Request password from stdin.
		const char *prompt = "Enter password: ";

		int res = write(1, prompt, strlen(prompt));

		if (res != (int)strlen(prompt)) {
			THROW("Failed to ask for password.");
		}
	}

	String buffer;

	while (buffer.Length() < 100000) {
		char c;

		int res = read(passFd, &c, 1);

		if (res <= 0 || c == '\n') {
			break;
		}

		buffer += c;
	}

	if (passFd) {
		close(passFd);
	}

	if (buffer.Length() == 0) {
		THROW("Empty passwords are not allowed.");
	}

	GenerateKeys(buffer.CStr());
	buffer.Wipe();
}

void Server::GenerateKeys(const char *password)
{
	uint8_t salt[SALT_SIZE];
	GetSalt("talkd.salt", salt);
	DeriveKey(password, salt, _privateKey);
	crypto_wipe(salt, SALT_SIZE);
	GeneratePublicKey(_privateKey, _publicKey);
}

void Server::WipeKeys()
{
	crypto_wipe(_privateKey, KEY_SIZE);
	crypto_wipe(_publicKey, KEY_SIZE);
}

void Server::OpenListeningSockets()
{
	_listeningSocket = socket(AF_INET, SOCK_STREAM, 0);

	if (_listeningSocket == -1) {
		THROW("Failed to create listening socket.");
	}

	uint16_t port = atoi(_configFile.Get("", "Port").CStr());

	if (port == 0) {
		THROW("Invalid port number.");
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	int res = inet_aton(
		_configFile.Get("", "IPv4").CStr(),
		&addr.sin_addr);

	if (!res) {
		THROW("Invalid IPv4 address.");
	}

	res = bind(_listeningSocket, (struct sockaddr*)&addr, sizeof(addr));

	if (res == -1) {
		THROW("Failed to bind listening socket.");
	}

	res = listen(_listeningSocket, 5);

	if (res == -1) {
		THROW("Failed to move socket to listening state.");
	}
}

void Server::CloseSockets()
{
	if (_listeningSocket != -1) {
		close(_listeningSocket);
	}
}

void Server::CloseSessions(ServerSession *sessions)
{
	while (sessions) {
		--_activeUsers;

		shutdown(sessions->Socket, SHUT_RDWR);
		close(sessions->Socket);

		if (sessions->Output) {
			delete sessions->Output;
		}

		if (sessions->Input) {
			delete sessions->Input;
		}

		crypto_wipe(sessions->OutES.Key, KEY_SIZE);
		crypto_wipe(sessions->InES.Key, KEY_SIZE);

		ServerSession *tmp = sessions;
		sessions = sessions->Next;
		delete tmp;
	}
}

void Server::AcceptConnection()
{
	int newSocket = accept(_listeningSocket, nullptr, nullptr);

	++_activeUsers;

	ServerSession *session = new ServerSession;

	session->Socket = newSocket;
	session->Time = GetUnixTime();
	session->Input = nullptr;
	session->ExpectedInput = 0;
	session->Output = nullptr;
	session->RequiredOutput = 0;

	session->Next = nullptr;
	session->Users = &_userDb;
	// session->Pipe = ...
	session->State = ServerSession::ServerStateWaitFirstSyn;
	session->SignatureKey = nullptr;
	session->PeerPublicKey = nullptr;
	session->PublicKey = _publicKey;
	session->PrivateKey = _privateKey;

	if (!_sessions) {
		_sessions = session;
	} else {
		ServerSession *list = _sessions;

		while (list->Next) {
			list = list->Next;
		}

		list->Next = session;
	}
}

struct pollfd *Server::BuildPollFds()
{
	struct pollfd *fds = new struct pollfd[_activeUsers + 1];

	// Listening socket.
	fds[0].fd = _listeningSocket;
	fds[0].events = POLLIN;

	ServerSession *session = _sessions;

	int index = 1;

	while (session) {
		fds[index].fd = session->Socket;

		if (session->Output) {
			fds[index].events = POLLIN | POLLOUT;
		} else {
			fds[index].events = POLLIN;
		}

		++index;
		session = session->Next;
	}

	return fds;
}
