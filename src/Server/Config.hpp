#ifndef _CONFIG_HPP
#define _CONFIG_HPP

#include "../Common/IniFile.hpp"

class ConfigUser
{
public:
	virtual ~ConfigUser()
	{ }

	virtual void ReloadConfig() = 0;
};

class Config
{
public:
	Config();
	~Config();

	void Reload();

	void RegisterConfigUser(ConfigUser *user);
	void UnregisterConfigUser(ConfigUser *user);

	uint32_t GetListeningAddress();
	uint16_t GetListeningPort();

	uint32_t GetGateAddress();
	uint16_t GetGatePort();

	uint64_t GetMessageSizeLimit();

	String GetHostName();

private:
	IniFile _configFile;

	void Init();
	void Validate();

	uint32_t _listeningAddress;
	uint16_t _listeningPort;

	uint32_t _gateAddress;
	uint16_t _gatePort;

	uint64_t _messageSizeLimit;

	String _hostName;

	struct ConfUser
	{
		ConfigUser *User;
		ConfUser *Next;
	};

	ConfUser *_confUsers;
};

#endif
