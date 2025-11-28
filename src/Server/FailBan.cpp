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
	return !_enabled || !Contains(ipv4);
}

void FailBan::Ban(uint32_t ipv4)
{
	int index;

	if (_freeIndices) {
		index = _freeIndices->Index;

		FreeIndex *tmp = _freeIndices;
		_freeIndices = _freeIndices->Next;
		delete tmp;
	} else {
		index = _file.Size() / sizeof(uint32_t);
		_file.Write<uint32_t>(&ipv4, 1, sizeof(uint32_t) * index);
	}

	Add(ipv4, index);

	Log(IpToString(ipv4) + " is banned.");
}

void FailBan::Add(uint32_t ip, int index)
{
	Entry **curr = &_db;

	while (*curr) {
		if (ip == (*curr)->IPv4) {
			return;
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
}

bool FailBan::Contains(uint32_t ip)
{
	Entry *curr = _db;

	while (curr) {
		if (ip == curr->IPv4) {
			return true;
		}

		if (ip < curr->IPv4) {
			curr = curr->Left;
		} else {
			curr = curr->Right;
		}
	}

	return false;
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
