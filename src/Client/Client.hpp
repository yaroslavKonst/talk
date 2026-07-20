#ifndef _CLIENT_HPP
#define _CLIENT_HPP

#include <cstdint>

#include "../Common/MyString.hpp"
#include "../Crypto/Crypto.hpp"

class Client
{
public:
	Client();
	~Client();

	int Run();

private:
	Crypto::X25519::PrivateKeyContainer _privateKey;
	Crypto::X25519::PublicKeyContainer _publicKey;

	void GetPassword();
	void GenerateKeys(const String &password);
};

#endif
