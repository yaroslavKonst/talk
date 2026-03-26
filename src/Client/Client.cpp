#include "Client.hpp"

#include <unistd.h>
#include <sys/stat.h>

#include "Root.hpp"
#include "Network.hpp"
#include "UI.hpp"
#include "ChatList.hpp"
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
	Root root;

	root.PrivateKey = _privateKey;
	root.PublicKey = _publicKey;

	EventDispatcher dispatcher(2000);
	root.Dispatcher = &dispatcher;

	Config config(_publicKey);
	root.Conf = &config;

	Network network(&root);
	root.Network = &network;

	ChatList chats(&root);
	root.Messages = &chats;

	UI ui(&root);
	root.Ui = &ui;

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
