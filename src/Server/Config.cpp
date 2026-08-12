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
static const char *IPSetting = "ClientIP";
static const char *IPSettingValue = "0.0.0.0";
static const char *PortSetting = "ClientPort";
static const char *PortSettingValue = "6524";
static const char *GateIPSetting = "GateIP";
static const char *GateIPSettingValue = "0.0.0.0";
static const char *GatePortSetting = "GatePort";
static const char *GatePortSettingValue = "6525";

static const char *LimitsSection = "Limits";
static const char *MessageSizeLimitSetting = "MessageSize";
static const char *MessageSizeLimitSettingValue = "50M";

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

static const char *SendPlannerSection = "SendPlanner";
static const char *SendPlannerRequestLimitDelay = "RequestRateLimitDelay";
static const char *SendPlannerRequestLimitDelayValue = "20s";
static const char *SendPlannerConnectionFailureDelay = "ConnectionFailureDelay";
static const char *SendPlannerConnectionFailureDelayValue = "30m";
static const char *SendPlannerMaxDeliveryTime = "MaxDeliveryTime";
static const char *SendPlannerMaxDeliveryTimeValue = "1w";

Config::Config(EventDispatcher *dispatcher) : _configFile("talkd.conf")
{
	_dispatcher = dispatcher;

	_listeningPort = 0;
	_gatePort = 0;
	_messageSizeLimit = 0;

	_confUsers = nullptr;

	Init();

	_dispatcher->RegisterSignalProcessor(this, SIGHUP);
}

Config::~Config()
{
	_dispatcher->UnregisterSignalProcessor(this, SIGHUP);

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

void Config::ProcessSignal(int signum)
{
	if (signum == SIGHUP) {
		ConfigLog("SIGHUP signal is received. Reloading.");
		Reload();
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

IPAddress Config::GetListeningAddress()
{
	return _listeningAddress;
}

uint16_t Config::GetListeningPort()
{
	return _listeningPort;
}

IPAddress Config::GetGateAddress()
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

int64_t Config::GetSendPlannerRequestLimitDelay()
{
	return _sendPlannerRequestLimitDelay;
}

int64_t Config::GetSendPlannerConnectionFailureDelay()
{
	return _sendPlannerConnectionFailureDelay;
}

int64_t Config::GetSendPlannerMaxDeliveryTime()
{
	return _sendPlannerMaxDeliveryTime;
}

void Config::Init()
{
	if (!FileExists(_configFile.GetPath())) {
		ConfigLog("Config file is not found. It will be created. "
			"Loading default configuration.");

		_configFile.Set(
			NetworkSection,
			HostNameSetting,
			HostNameSettingValue);
		_configFile.Set(NetworkSection, IPSetting, IPSettingValue);
		_configFile.Set(NetworkSection, PortSetting, PortSettingValue);
		_configFile.Set(
			NetworkSection,
			GateIPSetting,
			GateIPSettingValue);
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

		_configFile.Set(
			SendPlannerSection,
			SendPlannerRequestLimitDelay,
			SendPlannerRequestLimitDelayValue);
		_configFile.Set(
			SendPlannerSection,
			SendPlannerConnectionFailureDelay,
			SendPlannerConnectionFailureDelayValue);
		_configFile.Set(
			SendPlannerSection,
			SendPlannerMaxDeliveryTime,
			SendPlannerMaxDeliveryTimeValue);

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
	IPAddress addr;
	bool ipParseSuccess = addr.ParseIPAddress(
		_configFile.Get(NetworkSection, IPSetting));

	if (!ipParseSuccess) {
		THROW("Invalid IP address.");
	}

	// Gate port.
	int32_t gatePort = atoi(
		_configFile.Get(NetworkSection, GatePortSetting).CStr());

	if (gatePort <= 0 || gatePort > 65535) {
		THROW("Gate port number must be positive integer less "
			"than 65536.");
	}

	// Gate address.
	IPAddress gateAddr;
	ipParseSuccess = gateAddr.ParseIPAddress(
		_configFile.Get(NetworkSection, GateIPSetting));

	if (!ipParseSuccess) {
		THROW("Invalid gate IP address.");
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

	// Send planner.
	int64_t sendPlannerRequestLimitDelay;
	parseResult = ParseTime(
		_configFile.Get(
			SendPlannerSection,
			SendPlannerRequestLimitDelay),
		sendPlannerRequestLimitDelay);

	if (!parseResult || sendPlannerRequestLimitDelay <= 0) {
		THROW("SendPlanner request rate limit delay must be "
			"positive integer with optional "
			"s,m,h,d,w,M or Y suffix.");
	}

	int64_t sendPlannerConnectionFailureDelay;
	parseResult = ParseTime(
		_configFile.Get(
			SendPlannerSection,
			SendPlannerConnectionFailureDelay),
		sendPlannerConnectionFailureDelay);

	if (!parseResult || sendPlannerConnectionFailureDelay <= 0) {
		THROW("SendPlanner connection failure delay must be "
			"positive integer with optional "
			"s,m,h,d,w,M or Y suffix.");
	}

	int64_t sendPlannerMaxDeliveryTime;
	parseResult = ParseTime(
		_configFile.Get(
			SendPlannerSection,
			SendPlannerMaxDeliveryTime),
		sendPlannerMaxDeliveryTime);

	if (!parseResult || sendPlannerMaxDeliveryTime <= 0) {
		THROW("SendPlanner max delivery time must be "
			"positive integer with optional "
			"s,m,h,d,w,M or Y suffix.");
	}

	// Writing new parameters.
	_listeningAddress = addr;
	_listeningPort = port;
	ConfigLog("Client socket IP: " + _listeningAddress.ToString() + ".");
	ConfigLog("Client socket port: " + ToString(_listeningPort) + ".");

	_gateAddress = gateAddr;
	_gatePort = gatePort;
	ConfigLog("Gate socket IP: " + _gateAddress.ToString() + ".");
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
		TimeSpanInSecondsToString(_failBanBanTime) + " (" +
		ToString(_failBanBanTime) + " seconds).");
	ConfigLog("FailBan max tries: " + ToString(_failBanMaxTries) + ".");
	ConfigLog("FailBan cooldown interval: " +
		TimeSpanInSecondsToString(_failBanCooldownInterval) + " (" +
		ToString(_failBanCooldownInterval) + " seconds).");

	_rateLimiterMaxRequestsPerMinute = rateLimiterMaxRequestsPerMinute;
	_rateLimiterSessionTimeoutPenalty = rateLimiterSessionTimeoutPenalty;
	ConfigLog("RateLimiter max requests per minute: " +
		ToString(_rateLimiterMaxRequestsPerMinute) + ".");
	ConfigLog("RateLimiter timeout penalty: " +
		TimeSpanInSecondsToString(_rateLimiterSessionTimeoutPenalty) +
		" (" +
		ToString(_rateLimiterSessionTimeoutPenalty) + " seconds).");

	_sendPlannerRequestLimitDelay = sendPlannerRequestLimitDelay;
	_sendPlannerConnectionFailureDelay = sendPlannerConnectionFailureDelay;
	_sendPlannerMaxDeliveryTime = sendPlannerMaxDeliveryTime;
	ConfigLog("SendPlanner request rate limit delay: " +
		TimeSpanInSecondsToString(_sendPlannerRequestLimitDelay) +
		" (" +
		ToString(_sendPlannerRequestLimitDelay) + " seconds).");
	ConfigLog("SendPlanner connection failure delay: " +
		TimeSpanInSecondsToString(_sendPlannerConnectionFailureDelay) +
		" (" +
		ToString(_sendPlannerConnectionFailureDelay) + " seconds).");
	ConfigLog("SendPlanner max delivery time: " +
		TimeSpanInSecondsToString(_sendPlannerMaxDeliveryTime) +
		" (" +
		ToString(_sendPlannerMaxDeliveryTime) + " seconds).");
}

void Config::ConfigLog(String message)
{
	Log(LogLevel::Verbose, "Config", message);
}
