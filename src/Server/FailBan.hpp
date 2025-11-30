#ifndef _FAIL_BAN_HPP
#define _FAIL_BAN_HPP

#include "../Common/BinaryFile.hpp"
#include "../Common/CowBuffer.hpp"

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
	void Unban(uint32_t ipv4);

	CowBuffer<uint32_t> ListBanned();

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
	bool Add(uint32_t ip, int index);
	Entry **Find(uint32_t ip);
	void Remove(Entry **entry);
	int CountEntries(Entry *entry);
	void FillArray(Entry *entry, uint32_t *array, int *index);

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
