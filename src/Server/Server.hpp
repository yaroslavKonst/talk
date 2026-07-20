#ifndef _SERVER_HPP
#define _SERVER_HPP

#include <cstdint>

#include "../Crypto/Crypto.hpp"

class Server
{
public:
	Server();
	~Server();

	int Run();

private:
	Crypto::X25519::PrivateKeyContainer _privateKey;
	Crypto::X25519::PublicKeyContainer _publicKey;

	void GetPassword();
	void GenerateKeys(const char *password);
};

#endif
