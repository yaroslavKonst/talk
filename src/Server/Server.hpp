#ifndef _SERVER_HPP
#define _SERVER_HPP

#include "UserDB.hpp"
#include "Config.hpp"
#include "ListeningSocket.hpp"
#include "ControlSocket.hpp"
#include "../Common/EventDispatcher.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

class Server
{
public:
	Server();
	~Server();

	int Run();

private:
	EventDispatcher _dispatcher;
	Config _config;
	UserDB _users;
	ListeningSocket _listeningSocket;
	ControlSocket _controlSocket;

	uint8_t _privateKey[KEY_SIZE];
	uint8_t _publicKey[KEY_SIZE];

	void GetPassword();
	void GenerateKeys(const char *password);
	void WipeKeys();
};

#endif
