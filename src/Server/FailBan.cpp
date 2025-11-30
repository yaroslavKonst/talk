#include "FailBan.hpp"

#include <sys/socket.h>
#include <arpa/inet.h>

#include "../Common/Log.hpp"
#include "../Common/Debug.hpp"

static String IpToString(uint32_t ip)
{
	char ipStr[INET_ADDRSTRLEN];

	struct in_addr addr;
	addr.s_addr = ip;

	if (!inet_ntop(AF_INET, &addr, ipStr, INET_ADDRSTRLEN)) {
		return "IPv4";
	}

	return ipStr;
}

FailBan::FailBan() : _file("talkd.banned.ip", true)
{
	_enabled = false;
	_tries = 5;

	_cooldownTimestamp = GetUnixTime();

	_db = nullptr;
	_freeIndices = nullptr;

	Load();

	_counters = new Counter*[_CounterCount];
	memset(_counters, 0, sizeof(Counter*) * _CounterCount);
}

FailBan::~FailBan()
{
	for (int i = 0; i < _CounterCount; i++) {
		while (_counters[i]) {
			Counter *tmp = _counters[i];
			_counters[i] = _counters[i]->Next;
			delete tmp;
		}
	}

	delete[] _counters;

	Free();
}

void FailBan::SetEnabled(bool enabled)
{
	_enabled = enabled;
}

void FailBan::SetTries(int tries)
{
	_tries = tries;
}

void FailBan::RecordFailure(uint32_t ipv4)
{
	if (!_enabled) {
		return;
	}

	Log("Login failure from " + IpToString(ipv4) + ".");

	uint32_t tli = ipv4 >> 16;
	Counter **curr = &(_counters[tli]);

	while (*curr) {
		if ((*curr)->IPv4 == ipv4) {
			break;
		}

		curr = &((*curr)->Next);
	}

	if (!*curr) {
		*curr = new Counter;
		(*curr)->Next = nullptr;
		(*curr)->IPv4 = ipv4;
		(*curr)->FailureCount = 0;
	}

	(*curr)->FailureCount += 1;

	if ((*curr)->FailureCount > _tries) {
		Ban(ipv4);

		Counter *tmp = *curr;
		*curr = (*curr)->Next;
		delete tmp;
	}
}

int64_t FailBan::GetCooldownTimestamp()
{
	return _cooldownTimestamp;
}

void FailBan::Cooldown()
{
	_cooldownTimestamp = GetUnixTime();

	for (int i = 0; i < _CounterCount; i++) {
		Counter **curr = &(_counters[i]);

		while (*curr) {
			(*curr)->FailureCount -= 1;
			Log("Cooldown for " + IpToString((*curr)->IPv4) + ".");

			if ((*curr)->FailureCount <= 0) {
				Counter *tmp = *curr;
				*curr = (*curr)->Next;
				delete tmp;
			} else {
				curr = &((*curr)->Next);
			}
		}
	}
}

bool FailBan::IsAllowed(uint32_t ipv4)
{
	return !_enabled || !Find(ipv4);
}

bool FailBan::Ban(uint32_t ipv4)
{
	int index;

	if (_freeIndices) {
		index = _freeIndices->Index;

		FreeIndex *tmp = _freeIndices;
		_freeIndices = _freeIndices->Next;
		delete tmp;
	} else {
		index = _file.Size() / sizeof(uint32_t);
	}

	bool added = Add(ipv4, index);

	if (!added) {
		FreeIndex *entry = new FreeIndex;
		entry->Index = index;
		entry->Next = _freeIndices;
		_freeIndices = entry;
		return false;
	}

	_file.Write<uint32_t>(&ipv4, 1, sizeof(uint32_t) * index);
	Log(IpToString(ipv4) + " is banned.");

	return true;
}

bool FailBan::Unban(uint32_t ipv4)
{
	Entry **entry = Find(ipv4);

	if (!entry) {
		return false;
	}

	uint32_t zeroIP = 0;
	_file.Write<uint32_t>(
		&zeroIP,
		1,
		sizeof(uint32_t) *
		(*entry)->IndexInFile);

	FreeIndex *idx = new FreeIndex;
	idx->Index = (*entry)->IndexInFile;
	idx->Next = _freeIndices;
	_freeIndices = idx;

	Remove(entry);

	Log(IpToString(ipv4) + " is unbanned.");

	return true;
}

CowBuffer<uint32_t> FailBan::ListBanned()
{
	int bannedCount = CountEntries(_db);
	CowBuffer<uint32_t> result(bannedCount);

	int index = 0;
	FillArray(_db, result.Pointer(), &index);
	return result;
}

bool FailBan::Add(uint32_t ip, int index)
{
	Entry **curr = &_db;

	while (*curr) {
		if (ip == (*curr)->IPv4) {
			return false;
		}

		if (ip < (*curr)->IPv4) {
			curr = &((*curr)->Left);
		} else {
			curr = &((*curr)->Right);
		}
	}

	*curr = new Entry;
	(*curr)->IPv4 = ip;
	(*curr)->IndexInFile = index;

	return true;
}

FailBan::Entry **FailBan::Find(uint32_t ip)
{
	Entry **curr = &_db;

	while (*curr) {
		if (ip == (*curr)->IPv4) {
			return curr;
		}

		if (ip < (*curr)->IPv4) {
			curr = &((*curr)->Left);
		} else {
			curr = &((*curr)->Right);
		}
	}

	return nullptr;
}

void FailBan::Remove(Entry **entry)
{
	if (!(*entry)->Left) {
		Entry *tmp = *entry;
		*entry = (*entry)->Right;
		tmp->Left = nullptr;
		tmp->Right = nullptr;
		delete tmp;
	} else if (!(*entry)->Right) {
		Entry *tmp = *entry;
		*entry = (*entry)->Left;
		tmp->Left = nullptr;
		tmp->Right = nullptr;
		delete tmp;
	} else {
		Entry **lSubMax = &((*entry)->Left);

		while ((*lSubMax)->Right) {
			lSubMax = &((*lSubMax)->Right);
		}

		(*entry)->IPv4 = (*lSubMax)->IPv4;
		(*entry)->IndexInFile = (*lSubMax)->IndexInFile;

		Remove(lSubMax);
	}
}

int FailBan::CountEntries(Entry *entry)
{
	if (!entry) {
		return 0;
	}

	return 1 + CountEntries(entry->Left) + CountEntries(entry->Right);
}

void FailBan::FillArray(Entry *entry, uint32_t *array, int *index)
{
	if (!entry) {
		return;
	}

	FillArray(entry->Left, array, index);
	array[*index] = entry->IPv4;
	*index += 1;
	FillArray(entry->Right, array, index);
}

void FailBan::Load()
{
	int entryCount = _file.Size() / sizeof(uint32_t);

	for (int i = 0; i < entryCount; i++) {
		uint32_t ip;
		_file.Read<uint32_t>(&ip, 1, sizeof(uint32_t) * i);

		if (!ip) {
			FreeIndex *freeIndex = new FreeIndex;
			freeIndex->Index = i;
			freeIndex->Next = _freeIndices;
			_freeIndices = freeIndex;
		} else {
			Add(ip, i);
		}
	}
}

void FailBan::Free()
{
	if (_db) {
		delete _db;
		_db = nullptr;
	}

	while (_freeIndices) {
		FreeIndex *tmp = _freeIndices;
		_freeIndices = _freeIndices->Next;
		delete tmp;
	}
}
