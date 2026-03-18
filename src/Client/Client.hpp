#ifndef _CLIENT_HPP
#define _CLIENT_HPP

#include "Root.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

class Client
{
public:
	Client();
	~Client();

	int Run();

private:
	Root _root;

	uint8_t _privateKey[KEY_SIZE];
	uint8_t _publicKey[KEY_SIZE];
	void GetPassword();
	void GenerateKeys(const String &password);
};

#endif
