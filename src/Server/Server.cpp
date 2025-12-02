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

#include "../Protocol/ServerSession.hpp"
#include "../Protocol/ControlSession.hpp"
#include "../ServerCtl/SocketName.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/File.hpp"
#include "../Common/SignalHandling.hpp"
#include "../Common/Log.hpp"
#include "../Common/Debug.hpp"
#include "../Crypto/Crypto.hpp"

static const char *RestrictedModeSetting = "RestrictedMode";
static const char *RestrictedModeSettingValue = "No";

static const char *NetworkSection = "Network";
static const char *IPv4Setting = "IPv4";
static const char *IPv4SettingValue = "0.0.0.0";
static const char *PortSetting = "Port";
static const char *PortSettingValue = "6524";

static const char *FailBanSection = "FailBan";
static const char *FailBanEnabledSetting = "Enabled";
static const char *FailBanEnabledSettingValue = "No";
static const char *FailBanTriesSetting = "AllowedTries";
static const char *FailBanTriesSettingValue = "5";
static const char *FailBanCooldownSetting = "CooldownInterval";
static const char *FailBanCooldownSettingValue = "14400";

Server::Server() : _configFile("talkd.conf")
{
	umask(077);

	InitConfigFile();

	_sessionFirst = nullptr;

	_listeningSocket = -1;
	_controlSocket = -1;

	_activeUsers = 0;
	_work = false;
	_reload = false;
	_restrictedMode = false;

	LoadConfig();

	GetPassword();
}

Server::~Server()
{
	CloseSessions(_sessionFirst);
	CloseListeningSockets();
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
		int fdCount;
		struct pollfd *fds = BuildPollFds(fdCount);

		int res = poll(fds, fdCount, 10000);

		if (res == -1) {
			THROW("Error on poll.");
		}

		int64_t newTime = GetUnixTime();
		bool updateTime = newTime - currentTime >= 10;

		if (newTime - _failBan.GetCooldownTimestamp() >
			_failBanCooldownInterval)
		{
			_failBan.Cooldown();
		}

		if (updateTime) {
			currentTime = newTime;
		}

		ProcessPollFds(fds, updateTime);

		if (_reload) {
			_reload = false;
			ReloadConfigFile();
		}
	}

	return 0;
}

void Server::InitConfigFile()
{
	if (!FileExists(_configFile.GetPath())) {
		_configFile.Set(
			"",
			RestrictedModeSetting,
			RestrictedModeSettingValue);

		_configFile.Set(NetworkSection, IPv4Setting, IPv4SettingValue);
		_configFile.Set(NetworkSection, PortSetting, PortSettingValue);

		_configFile.Set(
			FailBanSection,
			FailBanEnabledSetting,
			FailBanEnabledSettingValue);
		_configFile.Set(
			FailBanSection,
			FailBanTriesSetting,
			FailBanTriesSettingValue);
		_configFile.Set(
			FailBanSection,
			FailBanCooldownSetting,
			FailBanCooldownSettingValue);

		_configFile.Write();
	}
}

void Server::LoadConfig()
{
	LoadRestrictedMode();
	LoadFailBan();
}

void Server::ReloadConfigFile()
{
	try {
		_configFile.Reload();

		LoadConfig();
		CloseUserSocket();
		OpenUserSocket();
	} catch (Exception &ex) {
		Log("Failed to reload config file.");
		Log(ex.Message());
		return;
	}

	Log("Reloaded config file.");
}

void Server::LoadFailBan()
{
	String enabledValue =
		_configFile.Get(FailBanSection, FailBanEnabledSetting);
	String triesValue =
		_configFile.Get(FailBanSection, FailBanTriesSetting);
	String intervalValue =
		_configFile.Get(FailBanSection, FailBanCooldownSetting);

	if (enabledValue == "Yes") {
		_failBan.SetEnabled(true);
	} else if (enabledValue == "No") {
		_failBan.SetEnabled(false);
	} else {
		THROW("Invalid FailBan.Enabled value. Expected 'Yes' or 'No'.");
	}

	int tries = atoi(triesValue.CStr());

	if (tries <= 0) {
		THROW("FailBan.AllowedTries value must be positive integer.");
	}

	int64_t cooldownInterval = atoi(intervalValue.CStr());

	if (cooldownInterval <= 0) {
		THROW("FailBan.CooldownInterval value must be positive "
			"integer.");
	}

	_failBan.SetTries(tries);
	_failBanCooldownInterval = cooldownInterval;
}

void Server::LoadRestrictedMode()
{
	String restrictedModeValue = _configFile.Get("", RestrictedModeSetting);

	if (restrictedModeValue == "Yes") {
		_restrictedMode = true;
	} else if (restrictedModeValue == "No") {
		_restrictedMode = false;
	} else {
		THROW("Invalid RestrictedMode value. "
			"Expected 'Yes' or 'No'.");
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
	OpenUserSocket();
	OpenControlSocket();
}

void Server::CloseListeningSockets()
{
	CloseUserSocket();
	CloseControlSocket();
}

void Server::OpenUserSocket()
{
	uint16_t port = atoi(
		_configFile.Get(NetworkSection, PortSetting).CStr());

	if (port == 0) {
		THROW("Invalid port number.");
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	int res = inet_aton(
		_configFile.Get(NetworkSection, IPv4Setting).CStr(),
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
		CloseUserSocket();
		THROW("Failed to bind listening socket.");
	}

	res = listen(_listeningSocket, 5);

	if (res == -1) {
		CloseUserSocket();
		THROW("Failed to move socket to listening state.");
	}
}

void Server::OpenControlSocket()
{
	_controlSocket = socket(AF_UNIX, SOCK_STREAM, 0);

	if (_controlSocket == -1) {
		THROW("Failed to create control socket.");
	}

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strncpy(
		addr.sun_path,
		TALKD_SOCKET_NAME,
		sizeof(addr.sun_path) - 1);

	int res = bind(_controlSocket, (struct sockaddr*)&addr, sizeof(addr));

	if (res == -1) {
		CloseControlSocket();
		THROW("Failed to bind control socket.");
	}

	res = listen(_controlSocket, 5);

	if (res == -1) {
		CloseControlSocket();
		THROW("Failed to move control socket to listening state.");
	}
}

void Server::CloseUserSocket()
{
	if (_listeningSocket != -1) {
		close(_listeningSocket);
		_listeningSocket = -1;
	}
}

void Server::CloseControlSocket()
{
	if (_controlSocket != -1) {
		close(_controlSocket);
		unlink(TALKD_SOCKET_NAME);
		_controlSocket = -1;
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
	struct sockaddr_in addr;
	unsigned int addrSize = sizeof(addr);

	int fd = accept(_listeningSocket, (struct sockaddr*)&addr, &addrSize);

	if (fd == -1) {
		return;
	}

	bool allowed = _failBan.IsAllowed(addr.sin_addr.s_addr);

	if (!allowed) {
		shutdown(fd, SHUT_RDWR);
		close(fd);
		return;
	}

	int flags = fcntl(fd, F_GETFL);

	if (flags == -1) {
		Log("Error: Failed to get socket flags.");
		shutdown(fd, SHUT_RDWR);
		close(fd);
		return;
	}

	flags = flags | O_NONBLOCK;
	int res = fcntl(fd, F_SETFL, flags);

	if (res == -1) {
		Log("Error: Failed to set NONBLOCK flag on socket.");
		shutdown(fd, SHUT_RDWR);
		close(fd);
		return;
	}

	++_activeUsers;

	ServerSession *session = new ServerSession;
	session->Socket = fd;

	session->Users = &_userDb;
	session->Pipe = &_pipe;
	session->Ban = &_failBan;
	session->IPv4 = addr.sin_addr.s_addr;
	session->RestrictedMode = &_restrictedMode;
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
	session->Ban = &_failBan;
	session->Work = &_work;
	session->Reload = &_reload;
	session->PublicKey = _publicKey;

	session->Next = _sessionFirst;
	_sessionFirst = session;
}

struct pollfd *Server::BuildPollFds(int &fdCount)
{
	int specialSocketCount =
		(_listeningSocket != -1 ? 1 : 0) +
		(_controlSocket != -1 ? 1 : 0);

	struct pollfd *fds = new struct pollfd[
		_activeUsers + specialSocketCount];

	int index = 0;

	if (_listeningSocket != -1) {
		fds[index].fd = _listeningSocket;
		fds[index].events = POLLIN;
		++index;
	}

	if (_controlSocket != -1) {
		fds[index].fd = _controlSocket;
		fds[index].events = POLLIN;
		++index;
	}

	Session *session = _sessionFirst;

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

	fdCount = index;

	return fds;
}

void Server::ProcessPollFds(struct pollfd *fds, bool updateTime)
{
	Session **session = &_sessionFirst;

	int index =
		(_listeningSocket != -1 ? 1 : 0) +
		(_controlSocket != -1 ? 1 : 0);

	while (*session)
	{
		bool endSession =
			(fds[index].revents & POLLERR) ||
			(fds[index].revents & POLLHUP) ||
			(fds[index].revents & POLLNVAL);

		if (!endSession && (fds[index].revents & POLLOUT)) {
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

	index = 0;

	if (_listeningSocket != -1) {
		if (fds[index].revents & POLLIN) {
			AcceptConnection();
		}

		++index;
	}

	if (_controlSocket != -1) {
		if (fds[index].revents & POLLIN) {
			AcceptControl();
		}

		++index;
	}

	delete[] fds;
}
