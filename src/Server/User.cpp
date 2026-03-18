#include "User.hpp"

#include <cstring>

#include "../Common/File.hpp"
#include "../Common/BinaryFile.hpp"
#include "../Common/Exception.hpp"
#include "../ThirdParty/monocypher.h"

void User::CreateUser(String name, const uint8_t publicKey[KEY_SIZE])
{
	String root = "storage/users/" + name;

	if (FileExists(root)) {
		THROW("User " + name + " already exists.");
	}

	StorePublicKey(root, publicKey);
}

void User::RemoveUser(String name)
{
	String root = "storage/users/" + name;
	THROW("Not implemented.");
}

User::User(String name)
{
	_root = "storage/users/" + name;

	if (!FileExists(_root)) {
		THROW("User " + name + " does not exist.");
	}

	_name = name;
	_sessions = nullptr;

	LoadPublicKey();
}

User::~User()
{
	crypto_wipe(_publicKey, KEY_SIZE);
}

void User::AddSession(int fd)
{
	THROW("Not omplemented.");
}

void User::LoadPublicKey()
{
	BinaryFile file(_root + "/key", false);

	if (file.Size() != KEY_SIZE) {
		THROW("Invalid public key size for user " + _name + ".");
	}

	file.Read(_publicKey, KEY_SIZE, 0);
}

void User::StorePublicKey(
	String root,
	const uint8_t publicKey[KEY_SIZE])
{
	BinaryFile file(root + "/key", true);
	file.Write(publicKey, KEY_SIZE, 0);
}
