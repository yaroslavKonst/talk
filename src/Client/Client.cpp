#include "Client.hpp"

#include <unistd.h>
#include <sys/stat.h>
#include <cstdio>

#include "Root.hpp"
#include "Network.hpp"
#include "UI.hpp"
#include "ChatList.hpp"
#include "../Common/SignalHandling.hpp"
#include "../Common/Exception.hpp"
#include "../Common/Hex.hpp"
#include "../Common/File.hpp"
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
}

int Client::Run()
{
	sigset_t processedSignals;
	sigemptyset(&processedSignals);
	sigaddset(&processedSignals, SIGWINCH);

	Root root;

	root.PrivateKey = &_privateKey;
	root.PublicKey = &_publicKey;

	EventDispatcher dispatcher(&processedSignals);
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
		res = read(0, &c, 1);

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

static void CheckAccountPresence(const Crypto::X25519::PublicKeyContainer &key)
{
	String path = "storage/" + DataToHex(key.Key, Crypto::X25519::KEY_SIZE);

	if (FileExists(path)) {
		return;
	}

	printf("Account with given key is not found. "
		"Create new account? [y/N] ");
	fflush(stdout);

	String answer;

	while (answer.Length() < 100) {
		char c;
		int res = read(0, &c, 1);

		if (res <= 0 || c == '\n') {
			break;
		}

		answer += c;
	}

	answer = answer.ToLowerCase();

	if (answer == "y" || answer == "yes") {
		return;
	}

	THROW("Account creation cancelled. Exit.");
}

void Client::GenerateKeys(const String &password)
{
	uint8_t salt[Crypto::X25519::SALT_SIZE];
	Crypto::X25519::GetSalt("talk.salt", salt);
	DeriveKey(password.CStr(), salt, _privateKey);
	crypto_wipe(salt, Crypto::X25519::SALT_SIZE);
	GeneratePublicKey(_privateKey, _publicKey);

	CheckAccountPresence(_publicKey);
}
