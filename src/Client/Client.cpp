#include "Client.hpp"

#include <unistd.h>
#include <sys/stat.h>

#include "Network.hpp"
#include "UI.hpp"
#include "../Common/SignalHandling.hpp"
#include "../Common/Exception.hpp"
#include "../Crypto/Crypto.hpp"
#include "../ThirdParty/monocypher.h"

Client::Client()
{
	umask(077);

	DisableSigPipe();
	GetPassword();
}

Client::~Client()
{
	crypto_wipe(_privateKey, KEY_SIZE);
	crypto_wipe(_publicKey, KEY_SIZE);
}

int Client::Run()
{
	_root.PrivateKey = _privateKey;
	_root.PublicKey = _publicKey;

	EventDispatcher dispatcher(2000);
	_root.Dispatcher = &dispatcher;

	Config config(_publicKey);
	_root.Conf = &config;

	Network network(&_root);
	_root.Network = &network;

	UI ui(&_root);
	_root.Ui = &ui;

	dispatcher.Run();

	return 0;
}

void Client::GetPassword()
{
	const char *prompt = "Enter password: ";
	int res = write(1, prompt, strlen(prompt));

	if (res != (int)strlen(prompt)) {
		THROW("Failed to ask for password.");
	}

	String password;

	while (password.Length() < 100000) {
		char c;
		int res = read(0, &c, 1);

		if (res <= 0 || c == '\n') {
			break;
		}

		password += c;
	}

	if (!password.Length()) {
		THROW("Empty password is not allowed.");
	}

	if (password.Length() >= 100000) {
		THROW("Password is too long.");
	}

	GenerateKeys(password);
	password.Wipe();
}

void Client::GenerateKeys(const String &password)
{
	uint8_t salt[SALT_SIZE];
	GetSalt("talk.salt", salt);
	DeriveKey(password.CStr(), salt, _privateKey);
	crypto_wipe(salt, SALT_SIZE);
	GeneratePublicKey(_privateKey, _publicKey);
}
