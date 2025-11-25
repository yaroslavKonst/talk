#include "Server.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <signal.h>

#include "../Protocol/ServerSession.hpp"
#include "../Protocol/ControlSession.hpp"
#include "../ServerCtl/SocketName.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/File.hpp"
#include "../Common/Debug.hpp"
#include "../Crypto/Crypto.hpp"

static void DisableSigPipe()
{
	struct sigaction act;
	memset(&act, 0, sizeof(act));
	act.sa_handler = SIG_IGN;

	int res = sigaction(SIGPIPE, &act, nullptr);

	if (res == -1) {
		THROW("Failed to ignore SIGPIPE signal.");
	}
}

Server::Server() : _configFile("talkd.conf")
{
	umask(077);

	InitConfigFile();

	_sessionFirst = nullptr;

	_listeningSocket = -1;
	_controlSocket = -1;

	_activeUsers = 0;
	_work = false;

	GetPassword();
}

Server::~Server()
{
	CloseSessions(_sessionFirst);
	CloseSockets();
	WipeKeys();
}

int Server::Run()
{
	_work = true;

	DisableSigPipe();
	OpenListeningSockets();

	_activeUsers = 0;

	int64_t currentTime = GetUnixTime();

	while (_work) {
		struct pollfd *fds = BuildPollFds();

		int res = poll(fds, _activeUsers + 2, 10000);

		if (res == -1) {
			THROW("Error on poll.");
		}

		int64_t newTime = GetUnixTime();
		bool updateTime = newTime - currentTime >= 10;

		if (updateTime) {
			currentTime = newTime;
		}

		ProcessPollFds(fds, updateTime);
	}

	return 0;
}

void Server::InitConfigFile()
{
	if (!FileExists(_configFile.GetPath())) {
		_configFile.Set("", "IPv4", "0.0.0.0");
		_configFile.Set("", "Port", "6524");
		_configFile.Write();
	}
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
		THROW("Empty password is not allowed.");
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
	// Listening socket.
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

	_listeningSocket = socket(AF_INET, SOCK_STREAM, 0);

	if (_listeningSocket == -1) {
		THROW("Failed to create listening socket.");
	}

	res = bind(_listeningSocket, (struct sockaddr*)&addr, sizeof(addr));

	if (res == -1) {
		THROW("Failed to bind listening socket.");
	}

	res = listen(_listeningSocket, 5);

	if (res == -1) {
		THROW("Failed to move socket to listening state.");
	}

	// Control socket.
	_controlSocket = socket(AF_UNIX, SOCK_STREAM, 0);

	if (_controlSocket == -1) {
		THROW("Failed to create control socket.");
	}

	struct sockaddr_un addr_un;
	addr_un.sun_family = AF_UNIX;
	strncpy(
		addr_un.sun_path,
		TALKD_SOCKET_NAME,
		sizeof(addr_un.sun_path) - 1);

	res = bind(_controlSocket, (struct sockaddr*)&addr_un, sizeof(addr_un));

	if (res == -1) {
		THROW("Failed to bind control socket.");
	}

	res = listen(_controlSocket, 5);

	if (res == -1) {
		THROW("Failed to move control socket to listening state.");
	}
}

void Server::CloseSockets()
{
	if (_listeningSocket != -1) {
		close(_listeningSocket);
	}

	if (_controlSocket != -1) {
		close(_controlSocket);
		unlink(TALKD_SOCKET_NAME);
	}
}

void Server::CloseSessions(Session *sessions)
{
	while (sessions) {
		--_activeUsers;

		Session *tmp = sessions;
		sessions = sessions->Next;
		delete tmp;
	}
}

void Server::AcceptConnection()
{
	int fd = accept(_listeningSocket, nullptr, nullptr);

	if (fd == -1) {
		return;
	}

	++_activeUsers;

	ServerSession *session = new ServerSession;
	session->Socket = fd;

	session->Users = &_userDb;
	session->Pipe = &_pipe;
	session->State = ServerSession::ServerStateWaitFirstSyn;
	session->SignatureKey = nullptr;
	session->PeerPublicKey = nullptr;
	session->PublicKey = _publicKey;
	session->PrivateKey = _privateKey;
	session->VoiceState = ServerSession::VoiceStateInactive;
	session->VoicePeer = nullptr;

	session->Next = _sessionFirst;
	_sessionFirst = session;
}

void Server::AcceptControl()
{
	int fd = accept(_controlSocket, nullptr, nullptr);

	if (fd == -1) {
		return;
	}

	++_activeUsers;

	ControlSession *session = new ControlSession;
	session->Socket = fd;

	session->Users = &_userDb;
	session->Work = &_work;
	session->PublicKey = _publicKey;

	session->Next = _sessionFirst;
	_sessionFirst = session;
}

struct pollfd *Server::BuildPollFds()
{
	struct pollfd *fds = new struct pollfd[_activeUsers + 2];

	// Listening socket.
	fds[0].fd = _listeningSocket;
	fds[0].events = POLLIN;

	fds[1].fd = _controlSocket;
	fds[1].events = POLLIN;

	Session *session = _sessionFirst;

	int index = 2;

	while (session) {
		fds[index].fd = session->Socket;

		if (session->CanWrite()) {
			fds[index].events = POLLIN | POLLOUT;
		} else {
			fds[index].events = POLLIN;
		}

		++index;
		session = session->Next;
	}

	return fds;
}

void Server::ProcessPollFds(struct pollfd *fds, bool updateTime)
{
	Session **session = &_sessionFirst;

	int index = 2;

	while (*session)
	{
		bool endSession = false;

		if (fds[index].revents & POLLOUT) {
			endSession = !(*session)->Write();
		}

		if (!endSession && (fds[index].revents & POLLIN)) {
			endSession = !(*session)->Read();
		}

		if (!endSession && (*session)->CanReceive()) {
			endSession = !(*session)->Process();
		}

		if (!endSession && updateTime) {
			endSession = !(*session)->TimePassed();
		}

		if (endSession) {
			Session *sessionToRm = *session;

			*session = (*session)->Next;

			sessionToRm->Next = nullptr;
			CloseSessions(sessionToRm);
		} else {
			session = &((*session)->Next);
		}

		++index;
	}

	if (fds[0].revents & POLLIN) {
		AcceptConnection();
	}

	if (fds[1].revents & POLLIN) {
		AcceptControl();
	}

	delete[] fds;
}
