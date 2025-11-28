#ifndef _FAIL_BAN_HPP
#define _FAIL_BAN_HPP

#include "../Common/BinaryFile.hpp"

// All IPv4 addresses are stored and processed in network byte order.
class FailBan
{
public:
	FailBan();
	~FailBan();

	void SetEnabled(bool enabled);
	void SetTries(int tries);

	void RecordFailure(uint32_t ipv4);

	int64_t GetCooldownTimestamp();
	void Cooldown();

	bool IsAllowed(uint32_t ipv4);
	void Ban(uint32_t ipv4);

private:
	struct Entry
	{
		uint32_t IPv4;
		int IndexInFile;

		Entry *Left;
		Entry *Right;

		Entry()
		{
			Left = nullptr;
			Right = nullptr;
		}

		~Entry()
		{
			if (Left) {
				delete Left;
			}

			if (Right) {
				delete Right;
			}
		}
	};

	struct FreeIndex
	{
		int Index;
		FreeIndex *Next;
	};

	Entry *_db;
	void Add(uint32_t ip, int index);
	bool Contains(uint32_t ip);

	FreeIndex *_freeIndices;

	bool _enabled;
	int _tries;

	BinaryFile _file;

	void Load();
	void Free();

	struct Counter
	{
		uint32_t IPv4;
		int FailureCount;

		Counter *Next;
	};

	const int _CounterCount = 65536;
	Counter **_counters;
	int64_t _cooldownTimestamp;
};

#endif
