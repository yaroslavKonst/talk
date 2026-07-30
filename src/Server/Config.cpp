#include "Config.hpp"

#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../Common/File.hpp"
#include "../Common/Exception.hpp"
#include "../Common/Log.hpp"

static const char *NetworkSection = "Network";
static const char *HostNameSetting = "HostName";
static const char *HostNameSettingValue = "localhost";
static const char *IPv4Setting = "ClientIPv4";
static const char *IPv4SettingValue = "0.0.0.0";
static const char *PortSetting = "ClientPort";
static const char *PortSettingValue = "6524";
static const char *GateIPv4Setting = "GateIPv4";
static const char *GateIPv4SettingValue = "0.0.0.0";
static const char *GatePortSetting = "GatePort";
static const char *GatePortSettingValue = "6525";

static const char *LimitsSection = "Limits";
static const char *MessageSizeLimitSetting = "MessageSize";
static const char *MessageSizeLimitSettingValue = "1G";

static const char *FailBanSection = "FailBan";
static const char *FailBanEnabledSetting = "Enabled";
static const char *FailBanEnabledSettingValue = "Yes";
static const char *FailBanMaxTriesSetting = "AllowedTries";
static const char *FailBanMaxTriesSettingValue = "5";
static const char *FailBanCooldownSetting = "CooldownInterval";
static const char *FailBanCooldownSettingValue = "4h";
static const char *FailBanBanTimeSetting = "BanInterval";
static const char *FailBanBanTimeSettingValue = "1d";

static const char *RateLimiterSection = "RateLimiter";
static const char *RateLimiterMaxRequestsPerMinute = "MaxRequestsPerMinute";
static const char *RateLimiterMaxRequestsPerMinuteValue = "60";
static const char *RateLimiterSessionTimeoutPenalty = "SessionTimeoutPenalty";
static const char *RateLimiterSessionTimeoutPenaltyValue = "10m";

Config::Config() : _configFile("talkd.conf")
{
	_listeningAddress = 0;
	_listeningPort = 0;
	_messageSizeLimit = 0;

	_confUsers = nullptr;

	Init();
}

Config::~Config()
{
	while (_confUsers) {
		ConfUser *tmp = _confUsers;
		_confUsers = _confUsers->Next;
		delete tmp;
	}
}

void Config::Reload()
{
	_configFile.Reload();
	Validate();

	ConfUser *node = _confUsers;

	while (node) {
		node->User->ReloadConfig();
		node = node->Next;
	}
}

void Config::RegisterConfigUser(ConfigUser *user)
{
	ConfUser *node = new ConfUser;
	node->Next = _confUsers;
	node->User = user;

	_confUsers = node;
}

void Config::UnregisterConfigUser(ConfigUser *user)
{
	ConfUser **u = &_confUsers;

	while (*u) {
		if ((*u)->User == user) {
			ConfUser *tmp = *u;
			*u = (*u)->Next;
			delete tmp;
		} else {
			u = &(*u)->Next;
		}
	}
}

uint32_t Config::GetListeningAddress()
{
	return _listeningAddress;
}

uint16_t Config::GetListeningPort()
{
	return _listeningPort;
}

uint32_t Config::GetGateAddress()
{
	return _gateAddress;
}

uint16_t Config::GetGatePort()
{
	return _gatePort;
}

uint64_t Config::GetMessageSizeLimit()
{
	return _messageSizeLimit;
}

String Config::GetHostName()
{
	return _hostName;
}

bool Config::GetFailBanEnabled()
{
	return _failBanEnabled;
}

int64_t Config::GetFailBanBanTime()
{
	return _failBanBanTime;
}

int32_t Config::GetFailBanMaxTries()
{
	return _failBanMaxTries;
}

int64_t Config::GetFailBanCooldownInterval()
{
	return _failBanCooldownInterval;
}

uint64_t Config::GetRateLimiterMaxRequestsPerMinute()
{
	return _rateLimiterMaxRequestsPerMinute;
}

uint64_t Config::GetRateLimiterSessionTimeoutPenalty()
{
	return _rateLimiterSessionTimeoutPenalty;
}

void Config::Init()
{
	if (!FileExists(_configFile.GetPath())) {
		_configFile.Set(
			NetworkSection,
			HostNameSetting,
			HostNameSettingValue);
		_configFile.Set(NetworkSection, IPv4Setting, IPv4SettingValue);
		_configFile.Set(NetworkSection, PortSetting, PortSettingValue);
		_configFile.Set(
			NetworkSection,
			GateIPv4Setting,
			GateIPv4SettingValue);
		_configFile.Set(
			NetworkSection,
			GatePortSetting,
			GatePortSettingValue);

		_configFile.Set(
			LimitsSection,
			MessageSizeLimitSetting,
			MessageSizeLimitSettingValue);

		_configFile.Set(
			FailBanSection,
			FailBanEnabledSetting,
			FailBanEnabledSettingValue);
		_configFile.Set(
			FailBanSection,
			FailBanMaxTriesSetting,
			FailBanMaxTriesSettingValue);
		_configFile.Set(
			FailBanSection,
			FailBanCooldownSetting,
			FailBanCooldownSettingValue);
		_configFile.Set(
			FailBanSection,
			FailBanBanTimeSetting,
			FailBanBanTimeSettingValue);

		_configFile.Set(
			RateLimiterSection,
			RateLimiterMaxRequestsPerMinute,
			RateLimiterMaxRequestsPerMinuteValue);
		_configFile.Set(
			RateLimiterSection,
			RateLimiterSessionTimeoutPenalty,
			RateLimiterSessionTimeoutPenaltyValue);

		_configFile.Write();
	}

	Validate();
}

static bool ParseYesNo(String text, bool &result)
{
	text = text.ToLowerCase();

	if (text == "yes") {
		result = true;
	} else if (text == "no") {
		result = false;
	} else {
		return false;
	}

	return true;
}

// Parses strings with pattern "DIGITS", "DIGITS{K,M,G}" or "DIGITS{k,m,g}"
// into size in bytes.
// K (k) -- kilobytes
// M (m) -- megabytes
// G (g) -- gigabytes
static bool ParseSize(String text, uint64_t &result)
{
	if (!text.Length()) {
		return false;
	}

	text = text.ToLowerCase();

	char lastChar = text.CStr()[text.Length() - 1];

	const uint64_t kilobyte = 1024;
	const uint64_t megabyte = kilobyte * 1024;
	const uint64_t gigabyte = megabyte * 1024;

	uint64_t multiplier = 1;

	if (lastChar == 'k') {
		multiplier = kilobyte;
	} else if (lastChar == 'm') {
		multiplier = megabyte;
	} else if (lastChar == 'g') {
		multiplier = gigabyte;
	} else if (lastChar < '0' || lastChar > '9') {
		return false;
	}

	if (multiplier > 1) {
		text = text.Substring(0, text.Length() - 1);
	}

	if (!text.Length()) {
		return false;
	}

	for (int i = 0; i < text.Length(); i++) {
		char c = text.CStr()[i];

		if (c < '0' || c > '9') {
			return false;
		}
	}

	int64_t res = atoll(text.CStr());

	if (res < 0) {
		return false;
	}

	result = res;
	result *= multiplier;

	return true;
}

// Parses strings with pattern "DIGITS" or "DIGITS{s,m,h,d,w,M,Y}".
// s -- seconds (optional, default)
// m -- minutes
// h -- hours
// d -- days
// w -- weeks (7 days)
// M -- months (30 days)
// Y -- years (365 days)
static bool ParseTime(String text, int64_t &result)
{
	if (!text.Length()) {
		return false;
	}

	char lastChar = text.CStr()[text.Length() - 1];

	const int64_t minute = 60;
	const int64_t hour = minute * 60;
	const int64_t day = hour * 24;
	const int64_t week = day * 7;
	const int64_t month = day * 30;
	const int64_t year = day * 365;

	int64_t multiplier = 1;

	if (lastChar == 's') {
		text = text.Substring(0, text.Length() - 1);
	} else if (lastChar == 'm') {
		multiplier = minute;
		text = text.Substring(0, text.Length() - 1);
	} else if (lastChar == 'h') {
		multiplier = hour;
		text = text.Substring(0, text.Length() - 1);
	} else if (lastChar == 'd') {
		multiplier = day;
		text = text.Substring(0, text.Length() - 1);
	} else if (lastChar == 'w') {
		multiplier = week;
		text = text.Substring(0, text.Length() - 1);
	} else if (lastChar == 'M') {
		multiplier = month;
		text = text.Substring(0, text.Length() - 1);
	} else if (lastChar == 'Y') {
		multiplier = year;
		text = text.Substring(0, text.Length() - 1);
	}

	if (!text.Length()) {
		return false;
	}

	for (int i = 0; i < text.Length(); i++) {
		char c = text.CStr()[i];

		if (c < '0' || c > '9') {
			bool allowChar =
				i == 0 &&
				text.Length() > 1 &&
				c == '-';

			if (!allowChar) {
				return false;
			}
		}
	}

	int64_t res = atoll(text.CStr());

	result = res;
	result *= multiplier;

	return true;
}

void Config::Validate()
{
	// Port.
	int32_t port = atoi(
		_configFile.Get(NetworkSection, PortSetting).CStr());

	if (port <= 0 || port > 65535) {
		THROW("Client port number must be positive integer less "
			"than 65536.");
	}

	// Address.
	struct in_addr addr;
	int res = inet_aton(
		_configFile.Get(NetworkSection, IPv4Setting).CStr(),
		&addr);

	if (!res) {
		THROW("Invalid IPv4 address.");
	}

	// Gate port.
	int32_t gatePort = atoi(
		_configFile.Get(NetworkSection, GatePortSetting).CStr());

	if (gatePort <= 0 || gatePort > 65535) {
		THROW("Gate port number must be positive integer less "
			"than 65536.");
	}

	// Gate address.
	struct in_addr gateAddr;
	res = inet_aton(
		_configFile.Get(NetworkSection, GateIPv4Setting).CStr(),
		&gateAddr);

	if (!res) {
		THROW("Invalid gate IPv4 address.");
	}

	// MessageSizeLimit.
	uint64_t messageSize;

	bool parseResult = ParseSize(
		_configFile.Get(LimitsSection, MessageSizeLimitSetting),
		messageSize);

	if (!parseResult || messageSize < 4096) {
		THROW("Message size limit must be integer with "
			"optional K,M or G suffix not less than 4096.");
	}

	// Host name.
	String hostName = _configFile.Get(NetworkSection, HostNameSetting);

	// FailBan.
	bool failBanEnabled;

	parseResult = ParseYesNo(
		_configFile.Get(FailBanSection, FailBanEnabledSetting),
		failBanEnabled);

	if (!parseResult) {
		THROW(String(FailBanSection) + "." + FailBanEnabledSetting +
			" must be 'Yes' or 'No'.");
	}

	int64_t failBanBanTime;
	parseResult = ParseTime(
		_configFile.Get(FailBanSection, FailBanBanTimeSetting),
		failBanBanTime);

	if (!parseResult || failBanBanTime <= 0) {
		THROW("FailBan ban time must be positive integer with "
			"optional s,m,h,d,w,M or Y suffix.");
	}

	int32_t failBanMaxTries = atoi(_configFile.Get(
		FailBanSection,
		FailBanMaxTriesSetting).CStr());

	if (failBanMaxTries <= 0) {
		THROW("FailBan max tries setting must be positive integer.");
	}

	int64_t failBanCooldownInterval;
	parseResult = ParseTime(
		_configFile.Get(FailBanSection, FailBanCooldownSetting),
		failBanCooldownInterval);

	if (!parseResult || failBanCooldownInterval <= 0) {
		THROW("FailBan cooldown setting must be positive integer "
			"with optional s,m,h,d,w,M or Y suffix.");
	}

	// Rate limiter.
	int64_t rateLimiterMaxRequestsPerMinute = atoll(_configFile.Get(
		RateLimiterSection,
		RateLimiterMaxRequestsPerMinute).CStr());

	if (rateLimiterMaxRequestsPerMinute <= 0) {
		THROW("RateLimiter max requests rate setting must be "
			"positive integer.");
	}

	int64_t rateLimiterSessionTimeoutPenalty;
	parseResult = ParseTime(
		_configFile.Get(
			RateLimiterSection,
			RateLimiterSessionTimeoutPenalty),
		rateLimiterSessionTimeoutPenalty);

	if (!parseResult || rateLimiterSessionTimeoutPenalty <= 0) {
		THROW("RateLimiter session timeout penalty setting must be "
			"positive integer with optional "
			"s,m,h,d,w,M or Y suffix.");
	}

	// Writing new parameters.
	_listeningAddress = addr.s_addr;
	_listeningPort = port;
	ConfigLog("Client socket IP: " + IPToString(_listeningAddress) + ".");
	ConfigLog("Client socket port: " + ToString(_listeningPort) + ".");

	_gateAddress = gateAddr.s_addr;
	_gatePort = gatePort;
	ConfigLog("Gate socket IP: " + IPToString(_gateAddress) + ".");
	ConfigLog("Gate socket port: " + ToString(_gatePort) + ".");

	_messageSizeLimit = messageSize;
	ConfigLog("Max message size: " +
		DataSizeToString(_messageSizeLimit) +
		" (" + ToString(_messageSizeLimit) + " bytes).");

	_hostName = hostName;
	ConfigLog("Host name: " + _hostName + ".");

	_failBanEnabled = failBanEnabled;
	_failBanBanTime = failBanBanTime;
	_failBanMaxTries = failBanMaxTries;
	_failBanCooldownInterval = failBanCooldownInterval;
	ConfigLog(String("FailBan enabled: ") +
		(_failBanEnabled ? "Yes" : "No") + ".");
	ConfigLog("FailBan ban time: " +
		ToString(_failBanBanTime) + " seconds.");
	ConfigLog("FailBan max tries: " + ToString(_failBanMaxTries) + ".");
	ConfigLog("FailBan cooldown interval: " +
		ToString(_failBanCooldownInterval) + " seconds.");

	_rateLimiterMaxRequestsPerMinute = rateLimiterMaxRequestsPerMinute;
	_rateLimiterSessionTimeoutPenalty = rateLimiterSessionTimeoutPenalty;
	ConfigLog("RateLimiter max requests per minute: " +
		ToString(_rateLimiterMaxRequestsPerMinute) + ".");
	ConfigLog("RateLimiter timeout penalty: " +
		ToString(_rateLimiterSessionTimeoutPenalty) + " seconds.");
}

void Config::ConfigLog(String message)
{
	Log("Config: " + message);
}
