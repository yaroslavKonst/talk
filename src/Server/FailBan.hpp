#ifndef _FAIL_BAN_HPP
#define _FAIL_BAN_HPP

#include "Config.hpp"
#include "../Common/EventDispatcher.hpp"
#include "../Common/BinaryFile.hpp"
#include "../Common/CowBuffer.hpp"
#include "../Common/Tree.hpp"

class FailBan :
	public ConfigUser,
	public TimeEventProcessor,
	public QuantEventProcessor
{
public:
	FailBan(EventDispatcher *dispatcher, Config *config);
	~FailBan();

	void RecordFailure(IPAddress ip);

	bool IsAllowed(IPAddress ip);
	bool Ban(IPAddress ip);
	bool Unban(IPAddress ip);

	CowBuffer<IPAddress> ListBanned();

	void ProcessTimeEvent() override;
	void ProcessQuant() override;
	void ReloadConfig() override;

private:
	EventDispatcher *_dispatcher;
	Config *_config;

	String _rootPath;

	struct BannedEntry
	{
		IPAddress IP;
		int64_t UnbanTime;

		BannedEntry()
		{
			UnbanTime = 0;
		}

		BannedEntry(IPAddress ip)
		{
			IP = ip;
			UnbanTime = 0;
		}

		bool operator==(const BannedEntry &e) const
		{
			return IP == e.IP;
		}

		bool operator<(const BannedEntry &e) const
		{
			return IP < e.IP;
		}
	};

	struct SuspiciousEntry
	{
		IPAddress IP;
		int32_t FailCount;
		int64_t ActionTime;

		SuspiciousEntry()
		{
			FailCount = 0;
			ActionTime = 0;
		}

		SuspiciousEntry(IPAddress ip)
		{
			IP = ip;
			FailCount = 0;
			ActionTime = 0;
		}

		bool operator==(const SuspiciousEntry &e) const
		{
			return IP == e.IP;
		}

		bool operator<(const SuspiciousEntry &e) const
		{
			return IP < e.IP;
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
