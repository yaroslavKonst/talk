#include "Server.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "ListeningSocket.hpp"
#include "ControlSocket.hpp"
#include "GateSession.hpp"
#include "SendPlanner.hpp"
#include "FailBan.hpp"
#include "RateLimiter.hpp"
#include "../Common/SignalHandling.hpp"
#include "../Common/Log.hpp"
#include "../Common/EventDispatcher.hpp"
#include "../Crypto/Crypto.hpp"

Server::Server()
{
	umask(077);
	GetPassword();
}

Server::~Server()
{
	Log("Core: Shutdown.");
}

int Server::Run()
{
	Log("Core: Startup.");

	DisableSigPipe();

	EventDispatcher dispatcher(10000);
	Config config;
	FailBan failBan(&dispatcher, &config);
	RateLimiter rateLimiter(&dispatcher, &config);
	UserDB users(&dispatcher, &config, &failBan, _privateKey, _publicKey);
	SendPlanner sendPlanner(&dispatcher, &users);
	ListeningSocket listeningSocket(&users, &dispatcher, &config, &failBan);
	ControlSocket controlSocket(
		&users,
		&dispatcher,
		&config,
		&failBan,
		_publicKey);
	GateListeningSocket gateSocket(&dispatcher, &config);

	listeningSocket.OpenSocket();
	controlSocket.OpenSocket();
	gateSocket.OpenSocket();

	dispatcher.Run();

	gateSocket.CloseSocket();
	controlSocket.CloseSocket();
	listeningSocket.CloseSocket();

	return 0;
}

void Server::GetPassword()
{
	// Password file.
	int passFd = open("talkd.shadow", O_RDONLY);

	if (passFd == -1) {
		passFd = 0;
		// Request password from stdin.
		const char *prompt = "Enter password: ";

		int res = write(1, prompt, strlen(prompt));

		if (res != (int)strlen(prompt)) {
			THROW("Failed to ask for password.");
		}
	}

	String buffer;

	while (buffer.Length() < 100000) {
		char c;

		int res = read(passFd, &c, 1);

		if (res <= 0 || c == '\n') {
			break;
		}

		buffer += c;
	}

	if (passFd) {
		close(passFd);
	}

	if (buffer.Length() == 0) {
		THROW("Empty password is not allowed.");
	}

	GenerateKeys(buffer.CStr());
	buffer.Wipe();
}

void Server::GenerateKeys(const char *password)
{
	uint8_t salt[Crypto::X25519::SALT_SIZE];
	Crypto::X25519::GetSalt("talkd.salt", salt);
	DeriveKey(password, salt, _privateKey);
	crypto_wipe(salt, Crypto::X25519::SALT_SIZE);
	GeneratePublicKey(_privateKey, _publicKey);
}
