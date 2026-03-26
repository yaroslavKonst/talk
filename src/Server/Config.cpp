#include "Config.hpp"

#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../Common/File.hpp"
#include "../Common/Exception.hpp"

static const char *NetworkSection = "Network";
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
static const char *MessageSizeLimitSettingValue = "1073741824";

static const char *FailBanSection = "FailBan";
static const char *FailBanEnabledSetting = "Enabled";
static const char *FailBanEnabledSettingValue = "Yes";
static const char *FailBanTriesSetting = "AllowedTries";
static const char *FailBanTriesSettingValue = "5";
static const char *FailBanCooldownSetting = "CooldownInterval";
static const char *FailBanCooldownSettingValue = "14400";
static const char *FailBanBanTimeSetting = "BanInterval";
static const char *FailBanBanTimeSettingValue = "86400";

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

void Config::Init()
{
	if (!FileExists(_configFile.GetPath())) {
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
			FailBanTriesSetting,
			FailBanTriesSettingValue);
		_configFile.Set(
			FailBanSection,
			FailBanCooldownSetting,
			FailBanCooldownSettingValue);
		_configFile.Set(
			FailBanSection,
			FailBanBanTimeSetting,
			FailBanBanTimeSettingValue);

		_configFile.Write();
	}

	Validate();
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
	int64_t messageSize = atoll(_configFile.Get(
		LimitsSection,
		MessageSizeLimitSetting).CStr());

	if (messageSize <= 0) {
		THROW("Message size limit must be positive integer.");
	}

	// Writing new parameters.
	_listeningAddress = addr.s_addr;
	_listeningPort = port;
	_gateAddress = gateAddr.s_addr;
	_gatePort = gatePort;
	_messageSizeLimit = messageSize;
}
