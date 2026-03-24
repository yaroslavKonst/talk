#include "Server.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "../Common/SignalHandling.hpp"
#include "../Crypto/Crypto.hpp"

Server::Server() :
	_dispatcher(10000),
	_users(&_dispatcher, &_config, _privateKey, _publicKey),
	_listeningSocket(&_users, &_dispatcher, &_config),
	_controlSocket(&_users, &_dispatcher, &_config, _publicKey)
{
	umask(077);
	GetPassword();
}

Server::~Server()
{
	WipeKeys();
}

int Server::Run()
{
	DisableSigPipe();

	_listeningSocket.OpenSocket();
	_controlSocket.OpenSocket();

	_dispatcher.Run();

	_controlSocket.CloseSocket();
	_listeningSocket.CloseSocket();

	return 0;
}

/*void Server::LoadFailBan()
{
	String enabledValue =
		_configFile.Get(FailBanSection, FailBanEnabledSetting);
	String triesValue =
		_configFile.Get(FailBanSection, FailBanTriesSetting);
	String intervalValue =
		_configFile.Get(FailBanSection, FailBanCooldownSetting);

	if (enabledValue == "Yes") {
		_failBan.SetEnabled(true);
	} else if (enabledValue == "No") {
		_failBan.SetEnabled(false);
	} else {
		THROW("Invalid FailBan.Enabled value. Expected 'Yes' or 'No'.");
	}

	int tries = atoi(triesValue.CStr());

	if (tries <= 0) {
		THROW("FailBan.AllowedTries value must be positive integer.");
	}

	int64_t cooldownInterval = atoi(intervalValue.CStr());

	if (cooldownInterval <= 0) {
		THROW("FailBan.CooldownInterval value must be positive "
			"integer.");
	}

	_failBan.SetTries(tries);
	_failBanCooldownInterval = cooldownInterval;
}*/

/*void Server::LoadRestrictedMode()
{
	String restrictedModeValue = _configFile.Get("", RestrictedModeSetting);

	if (restrictedModeValue == "Yes") {
		_restrictedMode = true;
	} else if (restrictedModeValue == "No") {
		_restrictedMode = false;
	} else {
		THROW("Invalid RestrictedMode value. "
			"Expected 'Yes' or 'No'.");
	}

}*/

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
	uint8_t salt[SALT_SIZE];
	GetSalt("talkd.salt", salt);
	DeriveKey(password, salt, _privateKey);
	crypto_wipe(salt, SALT_SIZE);
	GeneratePublicKey(_privateKey, _publicKey);
}

void Server::WipeKeys()
{
	crypto_wipe(_privateKey, KEY_SIZE);
	crypto_wipe(_publicKey, KEY_SIZE);
}



