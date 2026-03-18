#include "Config.hpp"

#include "../Common/File.hpp"

static const char *NetworkSection = "Network";
static const char *IPv4Setting = "IPv4";
static const char *IPv4SettingValue = "0.0.0.0";
static const char *PortSetting = "Port";
static const char *PortSettingValue = "6524";

static const char *FailBanSection = "FailBan";
static const char *FailBanEnabledSetting = "Enabled";
static const char *FailBanEnabledSettingValue = "No";
static const char *FailBanTriesSetting = "AllowedTries";
static const char *FailBanTriesSettingValue = "5";
static const char *FailBanCooldownSetting = "CooldownInterval";
static const char *FailBanCooldownSettingValue = "14400";

Config::Config() : _configFile("talkd.conf")
{
	Init();
}

void Config::Reload()
{
	_configFile.Reload();
}

String Config::GetListeningAddress()
{
	return _configFile.Get(NetworkSection, IPv4Setting);
}

String Config::GetListeningPort()
{
	return _configFile.Get(NetworkSection, PortSetting);
}

void Config::Init()
{
	if (!FileExists(_configFile.GetPath())) {
		_configFile.Set(NetworkSection, IPv4Setting, IPv4SettingValue);
		_configFile.Set(NetworkSection, PortSetting, PortSettingValue);

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

		_configFile.Write();
	}
}
