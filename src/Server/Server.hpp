#ifndef _SERVER_HPP
#define _SERVER_HPP

#include "UserDB.hpp"
#include "MessagePipe.hpp"
#include "FailBan.hpp"
#include "../Common/IniFile.hpp"
#include "../Protocol/Session.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

class Server
{
public:
	Server();
	~Server();

	int Run();

private:
	UserDB _userDb;
	MessagePipe _pipe;

	Session *_sessionFirst;

	bool _work;
	bool _reload;

	int _activeUsers;

	IniFile _configFile;
	void InitConfigFile();
	void LoadConfig();
	void ReloadConfigFile();

	FailBan _failBan;
	int64_t _failBanCooldownInterval;
	void LoadFailBan();

	bool _restrictedMode;
	void LoadRestrictedMode();

	uint8_t _privateKey[KEY_SIZE];
	uint8_t _publicKey[KEY_SIZE];

	int _listeningSocket;
	int _controlSocket;

	void GetPassword();
	void GenerateKeys(const char *password);
	void WipeKeys();

	void OpenListeningSockets();
	void CloseListeningSockets();
	void OpenUserSocket();
	void OpenControlSocket();
	void CloseUserSocket();
	void CloseControlSocket();

	void CloseSessions(Session *sessions);

	void AcceptConnection();
	void AcceptControl();

	struct pollfd *BuildPollFds(int &fdCount);
	void ProcessPollFds(struct pollfd *fds, bool updateTime);

};

#endif
