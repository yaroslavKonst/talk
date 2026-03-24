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

	uint64_t GetMessageSizeLimit();

private:
	IniFile _configFile;

	void Init();
	void Validate();

	uint32_t _listeningAddress;
	uint16_t _listeningPort;

	uint64_t _messageSizeLimit;

	struct ConfUser
	{
		ConfigUser *User;
		ConfUser *Next;
	};

	ConfUser *_confUsers;
};

#endif
