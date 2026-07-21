#ifndef _FAIL_BAN_HPP
#define _FAIL_BAN_HPP

#include "Config.hpp"
#include "../Common/EventDispatcher.hpp"
#include "../Common/BinaryFile.hpp"
#include "../Common/CowBuffer.hpp"
#include "../Common/Tree.hpp"

// All IPv4 addresses are stored and processed in network byte order.
class FailBan :
	public ConfigUser,
	public TimeEventProcessor,
	public QuantEventProcessor
{
public:
	FailBan(EventDispatcher *dispatcher, Config *config);
	~FailBan();

	void RecordFailure(uint32_t ipv4);

	bool IsAllowed(uint32_t ipv4);
	bool Ban(uint32_t ipv4);
	bool Unban(uint32_t ipv4);

	CowBuffer<uint32_t> ListBanned();

	void ProcessTimeEvent() override;
	void ProcessQuant() override;
	void ReloadConfig() override;

private:
	EventDispatcher *_dispatcher;
	Config *_config;

	String _rootPath;

	struct BannedEntry
	{
		uint32_t IPv4;
		int64_t UnbanTime;

		BannedEntry()
		{
			IPv4 = 0;
			UnbanTime = 0;
		}

		BannedEntry(uint32_t ipv4)
		{
			IPv4 = ipv4;
			UnbanTime = 0;
		}

		bool operator==(const BannedEntry &e) const
		{
			return IPv4 == e.IPv4;
		}

		bool operator<(const BannedEntry &e) const
		{
			return IPv4 < e.IPv4;
		}
	};

	struct SuspiciousEntry
	{
		uint32_t IPv4;
		int32_t FailCount;
		int64_t ActionTime;

		SuspiciousEntry()
		{
			IPv4 = 0;
			FailCount = 0;
			ActionTime = 0;
		}

		SuspiciousEntry(uint32_t ipv4)
		{
			IPv4 = ipv4;
			FailCount = 0;
			ActionTime = 0;
		}

		bool operator==(const SuspiciousEntry &e) const
		{
			return IPv4 == e.IPv4;
		}

		bool operator<(const SuspiciousEntry &e) const
		{
			return IPv4 < e.IPv4;
		}
	};

	Tree<BannedEntry> _bannedAddresses;
	Tree<SuspiciousEntry> _suspiciousAddresses;

	Tree<BannedEntry>::Entry *_traverseBannedEntry;
	Tree<SuspiciousEntry>::Entry *_traverseSuspiciousEntry;

	bool _enabled;
	int _tries;
	int64_t _banTime;
	int64_t _cooldownInterval;

	void Load();
	void LoadConfig();

	void CheckBanned();
	void CheckSuspicious();

	void FailBanLog(String message);
};

#endif
