#include "FailBan.hpp"

#include <sys/socket.h>
#include <arpa/inet.h>

#include "../Common/File.hpp"
#include "../Common/Log.hpp"
#include "../Common/Debug.hpp"

FailBan::FailBan(EventDispatcher *dispatcher, Config *config)
{
	_rootPath = "storage/FailBan";

	SetTimestamp(GetUnixTime());

	_dispatcher = dispatcher;
	_config = config;

	Load();
	LoadConfig();

	_dispatcher->RegisterTimeProcessor(this);
	_config->RegisterConfigUser(this);
}

FailBan::~FailBan()
{
	_config->UnregisterConfigUser(this);
	_dispatcher->UnregisterTimeProcessor(this);
}

void FailBan::RecordFailure(uint32_t ipv4)
{
	if (!_enabled) {
		return;
	}

	Tree<SuspiciousEntry>::Entry *entry =
		_suspiciousAddresses.FindEntry(ipv4);

	if (!entry) {
		_suspiciousAddresses.AddEntry(ipv4);
		entry = _suspiciousAddresses.FindEntry(ipv4);
	}

	entry->Key.FailCount += 1;
	entry->Key.ActionTime = GetUnixTime();

	FailBanLog("Login failure from " + IPToString(ipv4) +
		". Fails: " + ToString(entry->Key.FailCount) + ".");

	if (entry->Key.FailCount < _tries) {
		return;
	}

	_suspiciousAddresses.RemoveEntry(entry);

	Ban(ipv4);
}

bool FailBan::IsAllowed(uint32_t ipv4)
{
	return !_enabled || !_bannedAddresses.FindEntry(ipv4);
}

bool FailBan::Ban(uint32_t ipv4)
{
	BannedEntry entry;
	entry.IPv4 = ipv4;
	entry.UnbanTime = GetUnixTime() + _banTime;

	if (_bannedAddresses.FindEntry(entry)) {
		return false;
	}

	BinaryFile file(_rootPath + "/" + IPToString(ipv4), true);
	file.Write<int64_t>(&entry.UnbanTime, 1, 0);

	_bannedAddresses.AddEntry(entry);

	FailBanLog(IPToString(ipv4) + " is banned.");

	return true;
}

bool FailBan::Unban(uint32_t ipv4)
{
	Tree<BannedEntry>::Entry *entry = _bannedAddresses.FindEntry(ipv4);

	if (!entry) {
		return false;
	}

	DeleteFile(_rootPath + "/" + IPToString(ipv4));

	_bannedAddresses.RemoveEntry(entry);

	FailBanLog(IPToString(ipv4) + " is unbanned.");

	return true;
}

CowBuffer<uint32_t> FailBan::ListBanned()
{
	int bannedCount = 0;

	for (Tree<BannedEntry>::Entry *entry = _bannedAddresses.FindSmallest();
		entry;
		entry = _bannedAddresses.Next(entry))
	{
		++bannedCount;
	}

	CowBuffer<uint32_t> result(bannedCount);

	int index = 0;

	for (Tree<BannedEntry>::Entry *entry = _bannedAddresses.FindSmallest();
		entry;
		entry = _bannedAddresses.Next(entry))
	{
		result[index] = entry->Key.IPv4;
		++index;
	}

	return result;
}

void FailBan::ProcessTimeEvent()
{
	CheckBanned();
	CheckSuspicious();
}

void FailBan::ReloadConfig()
{
	LoadConfig();
}

void FailBan::Load()
{
	if (!FileExists(_rootPath)) {
		CreateDirectory("storage");
		CreateDirectory(_rootPath);
	}

	CowBuffer<String> ipStrings = ListDirectory(_rootPath);

	for (unsigned int i = 0; i < ipStrings.Size(); i++) {
		BannedEntry entry;

		struct in_addr addr;
		int res = inet_aton(
			ipStrings[i].CStr(),
			&addr);

		if (!res) {
			THROW("Invalid IPv4 address in FailBan DB.");
		}

		entry.IPv4 = addr.s_addr;

		BinaryFile file(_rootPath + "/" + ipStrings[i], false);
		file.Read<int64_t>(&entry.UnbanTime, 1, 0);

		_bannedAddresses.AddEntry(entry);
	}
}

void FailBan::LoadConfig()
{
	_enabled = _config->GetFailBanEnabled();
	_tries = _config->GetFailBanMaxTries();
	_banTime = _config->GetFailBanBanTime();
	_cooldownInterval = _config->GetFailBanCooldownInterval();

	SetInterval(
		_banTime < _cooldownInterval ? _banTime : _cooldownInterval);
}

void FailBan::CheckBanned()
{
	Tree<BannedEntry>::Entry *entry = _bannedAddresses.FindSmallest();

	while (entry) {
		if (entry->Key.UnbanTime <= GetUnixTime()) {
			uint32_t ip = entry->Key.IPv4;
			entry = _bannedAddresses.Next(entry);
			Unban(ip);
		} else {
			entry = _bannedAddresses.Next(entry);
		}
	}
}

void FailBan::CheckSuspicious()
{
	Tree<SuspiciousEntry>::Entry *entry =
		_suspiciousAddresses.FindSmallest();

	while (entry) {
		Tree<SuspiciousEntry>::Entry *current = entry;
		entry = _suspiciousAddresses.Next(entry);

		if (current->Key.ActionTime + _cooldownInterval > GetUnixTime())
		{
			continue;
		}

		if (current->Key.FailCount > 1) {
			current->Key.FailCount -= 1;
			current->Key.ActionTime = GetUnixTime();

			FailBanLog("Cooldown for " +
				IPToString(current->Key.IPv4) +
				". Fails: " +
				ToString(current->Key.FailCount) + ".");
		} else {
			FailBanLog("Cooldown for " +
				IPToString(current->Key.IPv4) +
				". Address is clear.");

			_suspiciousAddresses.RemoveEntry(current);
		}
	}
}

void FailBan::FailBanLog(String message)
{
	Log("FailBan: " + message);
}
