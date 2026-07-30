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

	bool GetFailBanEnabled();
	int64_t GetFailBanBanTime();
	int32_t GetFailBanMaxTries();
	int64_t GetFailBanCooldownInterval();

	uint64_t GetRateLimiterMaxRequestsPerMinute();
	uint64_t GetRateLimiterSessionTimeoutPenalty();

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

	bool _failBanEnabled;
	int64_t _failBanBanTime;
	int32_t _failBanMaxTries;
	int64_t _failBanCooldownInterval;

	uint64_t _rateLimiterMaxRequestsPerMinute;
	uint64_t _rateLimiterSessionTimeoutPenalty;

	struct ConfUser
	{
		ConfigUser *User;
		ConfUser *Next;
	};

	ConfUser *_confUsers;
};

#endif
