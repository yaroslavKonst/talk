#ifndef _SERVER_HPP
#define _SERVER_HPP

#include <cstdint>

#include "../Crypto/CryptoDefinitions.hpp"

class Server
{
public:
	Server();
	~Server();

	int Run();

private:
	uint8_t _privateKey[KEY_SIZE];
	uint8_t _publicKey[KEY_SIZE];

	void GetPassword();
	void GenerateKeys(const char *password);
	void WipeKeys();
};

#endif
