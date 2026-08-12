#include "FailBan.hpp"

#include <sys/socket.h>
#include <arpa/inet.h>

#include "../Common/File.hpp"
#include "../Common/UnixTime.hpp"
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

void FailBan::RecordFailure(IPAddress ip)
{
	if (!_enabled) {
		return;
	}

	Tree<SuspiciousEntry>::Entry *entry =
		_suspiciousAddresses.FindEntry(ip);

	if (!entry) {
		_suspiciousAddresses.AddEntry(ip);
		entry = _suspiciousAddresses.FindEntry(ip);
	}

	entry->Key.FailCount += 1;
	entry->Key.ActionTime = GetUnixTime();

	FailBanLog("Login failure from " + ip.ToString() +
		". Fails: " + ToString(entry->Key.FailCount) + ".");

	if (entry->Key.FailCount < _tries) {
		return;
	}

	if (_traverseSuspiciousEntry == entry) {
		_traverseSuspiciousEntry =
			_suspiciousAddresses.Next(_traverseSuspiciousEntry);
	}

	_suspiciousAddresses.RemoveEntry(entry);

	Ban(ip);
}

bool FailBan::IsAllowed(IPAddress ip)
{
	return !_enabled || !_bannedAddresses.FindEntry(ip);
}

bool FailBan::Ban(IPAddress ip)
{
	BannedEntry entry;
	entry.IP = ip;
	entry.UnbanTime = GetUnixTime() + _banTime;

	if (_bannedAddresses.FindEntry(entry)) {
		return false;
	}

	BinaryFile file(_rootPath + "/" + ip.ToString(), true);
	file.Write<int64_t>(&entry.UnbanTime, 1, 0);

	_bannedAddresses.AddEntry(entry);

	FailBanLog(ip.ToString() + " is banned.");

	return true;
}

bool FailBan::Unban(IPAddress ip)
{
	Tree<BannedEntry>::Entry *entry = _bannedAddresses.FindEntry(ip);

	if (!entry) {
		return false;
	}

	DeleteFile(_rootPath + "/" + ip.ToString());

	if (_traverseBannedEntry == entry) {
		_traverseBannedEntry =
			_bannedAddresses.Next(_traverseBannedEntry);
	}

	_bannedAddresses.RemoveEntry(entry);

	FailBanLog(ip.ToString() + " is unbanned.");

	return true;
}

CowBuffer<IPAddress> FailBan::ListBanned()
{
	int bannedCount = 0;

	for (Tree<BannedEntry>::Entry *entry = _bannedAddresses.FindSmallest();
		entry;
		entry = _bannedAddresses.Next(entry))
	{
		++bannedCount;
	}

	CowBuffer<IPAddress> result(bannedCount);

	int index = 0;

	for (Tree<BannedEntry>::Entry *entry = _bannedAddresses.FindSmallest();
		entry;
		entry = _bannedAddresses.Next(entry))
	{
		result[index] = entry->Key.IP;
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

		bool res = entry.IP.ParseIPAddress(ipStrings[i]);

		if (!res) {
			THROW("Invalid IP address in FailBan DB.");
		}

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
		IPAddress ip = entry->Key.IP;
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
			entry->Key.IP.ToString() +
			". Fails: " +
			ToString(entry->Key.FailCount) + ".");
	} else {
		FailBanLog("Cooldown for " +
			entry->Key.IP.ToString() +
			". Address is clear.");

		_suspiciousAddresses.RemoveEntry(entry);
	}
}

void FailBan::FailBanLog(String message)
{
	Log(LogLevel::Info, "FailBan", message);
}
