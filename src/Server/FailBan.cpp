#include "FailBan.hpp"

#include <sys/socket.h>
#include <arpa/inet.h>

#include "../Common/File.hpp"
#include "../Common/Log.hpp"
#include "../Common/Debug.hpp"

FailBan::FailBan(EventDispatcher *dispatcher, Config *config)
{
	_rootPath = "storage/FailBan";

	SetTimestamp(GetMonotonicMillisecondTime());

	_dispatcher = dispatcher;
	_config = config;

	_traverseBannedEntry = nullptr;
	_traverseSuspiciousEntry = nullptr;

	Load();
	LoadConfig();

	_dispatcher->RegisterTimeProcessor(this);
	_config->RegisterConfigUser(this);
}

FailBan::~FailBan()
{
	_config->UnregisterConfigUser(this);
	_dispatcher->UnregisterQuantProcessor(this);
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

	if (_traverseSuspiciousEntry == entry) {
		_traverseSuspiciousEntry =
			_suspiciousAddresses.Next(_traverseSuspiciousEntry);
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

	if (_traverseBannedEntry == entry) {
		_traverseBannedEntry =
			_bannedAddresses.Next(_traverseBannedEntry);
	}

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
	if (_traverseBannedEntry || _traverseSuspiciousEntry) {
		return;
	}

	_traverseBannedEntry = _bannedAddresses.FindSmallest();
	_traverseSuspiciousEntry = _suspiciousAddresses.FindSmallest();

	if (_traverseBannedEntry || _traverseSuspiciousEntry) {
		_dispatcher->RegisterQuantProcessor(this);
	}
}

void FailBan::ProcessQuant()
{
	CheckBanned();
	CheckSuspicious();

	if (_traverseBannedEntry || _traverseSuspiciousEntry) {
		_dispatcher->RegisterQuantProcessor(this);
	}
}

void FailBan::ReloadConfig()
{
	LoadConfig();
	_dispatcher->UnregisterTimeProcessor(this);
	_dispatcher->RegisterTimeProcessor(this);
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

	ProcessTimeEvent();
}

void FailBan::LoadConfig()
{
	_enabled = _config->GetFailBanEnabled();
	_tries = _config->GetFailBanMaxTries();
	_banTime = _config->GetFailBanBanTime();
	_cooldownInterval = _config->GetFailBanCooldownInterval();

	SetInterval(
		(_banTime < _cooldownInterval ? _banTime : _cooldownInterval) *
		1000);
}

void FailBan::CheckBanned()
{
	if (!_traverseBannedEntry) {
		return;
	}

	Tree<BannedEntry>::Entry *entry = _traverseBannedEntry;
	_traverseBannedEntry = _bannedAddresses.Next(_traverseBannedEntry);

	if (entry->Key.UnbanTime <= GetUnixTime()) {
		uint32_t ip = entry->Key.IPv4;
		Unban(ip);
	}
}

void FailBan::CheckSuspicious()
{
	if (!_traverseSuspiciousEntry) {
		return;
	}

	Tree<SuspiciousEntry>::Entry *entry = _traverseSuspiciousEntry;
	_traverseSuspiciousEntry =
		_suspiciousAddresses.Next(_traverseSuspiciousEntry);

	if (entry->Key.ActionTime + _cooldownInterval > GetUnixTime())
	{
		return;
	}

	if (entry->Key.FailCount > 1) {
		entry->Key.FailCount -= 1;
		entry->Key.ActionTime = GetUnixTime();

		FailBanLog("Cooldown for " +
			IPToString(entry->Key.IPv4) +
			". Fails: " +
			ToString(entry->Key.FailCount) + ".");
	} else {
		FailBanLog("Cooldown for " +
			IPToString(entry->Key.IPv4) +
			". Address is clear.");

		_suspiciousAddresses.RemoveEntry(entry);
	}
}

void FailBan::FailBanLog(String message)
{
	Log("FailBan", message);
}
