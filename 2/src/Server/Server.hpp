#ifndef _SERVER_HPP
#define _SERVER_HPP

#include "../Common/IniFile.hpp"
#include "../Common/CowBuffer.hpp"
#include "../Crypto/Crypto.hpp"
#include "../Protocol/Protocol.hpp"

class Server
{
public:
	Server();
	~Server();

	int Run();

private:
	UserDB _userDb;

	ServerSession *_sessions;
	int _activeUsers;

	IniFile _configFile;

	uint8_t _privateKey[KEY_SIZE];
	uint8_t _publicKey[KEY_SIZE];

	int _listeningSocket;

	void GetPassword();
	void GenerateKeys(const char *password);
	void WipeKeys();

	void OpenListeningSockets();
	void CloseSockets();

	void CloseSessions(ServerSession *sessions);
	void AcceptConnection();

	struct pollfd *BuildPollFds();
};

#endif
