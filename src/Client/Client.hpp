#ifndef _CLIENT_HPP
#define _CLIENT_HPP

#include <cstdint>

#include "../Common/MyString.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

class Client
{
public:
	Client();
	~Client();

	int Run();

private:
	uint8_t _privateKey[KEY_SIZE];
	uint8_t _publicKey[KEY_SIZE];

	void GetPassword();
	void GenerateKeys(const String &password);
};

#endif
