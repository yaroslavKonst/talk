#ifndef _SERVER_HPP
#define _SERVER_HPP

#include "UserDB.hpp"
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

	Session *_sessionFirst;
	Session *_sessionLast;

	bool _work;

	int _activeUsers;

	IniFile _configFile;

	uint8_t _privateKey[KEY_SIZE];
	uint8_t _publicKey[KEY_SIZE];

	int _listeningSocket;
	int _controlSocket;

	void GetPassword();
	void GenerateKeys(const char *password);
	void WipeKeys();

	void OpenListeningSockets();
	void CloseSockets();

	void CloseSessions(Session *sessions);

	void AcceptConnection();
	void AcceptControl();

	struct pollfd *BuildPollFds();
	void ProcessPollFds(struct pollfd *fds, bool updateTime);

};

#endif
