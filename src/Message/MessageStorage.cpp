#include "MessageStorage.hpp"

#include "../Common/Hex.hpp"
#include "../Common/BinaryFile.hpp"
#include "../Common/UnixTime.hpp"
#include "../Crypto/CryptoDefinitions.hpp"
#include "../ThirdParty/monocypher.h"

// Index.
MessageStorageIndex::MessageStorageIndex(String path) :
	_file(path, true),
	_cache(&_file)
{
	if (_file.Size() == 0) {
		IndexEntry entry;
		memset(&entry, 0, sizeof(entry));
		_file.Write<IndexEntry>(&entry, 1, 0);
	}
}

bool MessageStorageIndex::EntryExists(
	int64_t timestamp,
	int32_t index,
	bool incoming)
{
	IndexEntry indexEntry = _cache[0];

	int64_t currentAddress = indexEntry.Right;

	EntryValue value;
	value.Timestamp = timestamp;
	value.Index = index;
	value.Incoming = incoming ? 1 : 0;

	while (currentAddress) {
		indexEntry = _cache[currentAddress];

		if (value == indexEntry.Value) {
			return indexEntry.Valid;
		}

		if (value < indexEntry.Value) {
			currentAddress = indexEntry.Left;
		} else {
			currentAddress = indexEntry.Right;
		}
	}

	return false;
}

void MessageStorageIndex::AddEntry(
	int64_t timestamp,
	int32_t index,
	bool incoming)
{
	EntryValue value;
	value.Timestamp = timestamp;
	value.Index = index;
	value.Incoming = incoming ? 1 : 0;

	IndexEntry parentEntry = _cache[0];

	uint32_t *currentAddress = &parentEntry.Right;

	while (*currentAddress) {
		IndexEntry entry = _cache[*currentAddress];

		if (value == entry.Value) {
			if (!entry.Valid) {
				entry.Valid = 1;
				_cache[*currentAddress] = entry;
			}

			return;
		}

		if (value < entry.Value) {
			currentAddress = &parentEntry.Left;
		} else {
			currentAddress = &parentEntry.Right;
		}

		parentEntry = entry;
	}

	uint32_t parentAddress = parentEntry.This;
	uint32_t newAddress = Allocate();
	*currentAddress = newAddress;
	_cache[parentEntry.This] = parentEntry;

	memset(&parentEntry, 0, sizeof(parentEntry));
	parentEntry.Value = value;
	parentEntry.Valid = 1;
	parentEntry.This = newAddress;
	parentEntry.Parent = parentAddress;
	parentEntry.Depth = 0;

	_cache[newAddress] = parentEntry;

	Rollup(newAddress);
}

void MessageStorageIndex::RemoveEntry(
	int64_t timestamp,
	int32_t index,
	bool incoming)
{
	EntryValue value;
	value.Timestamp = timestamp;
	value.Index = index;
	value.Incoming = incoming ? 1 : 0;

	IndexEntry parentEntry = _cache[0];

	uint32_t currentAddress = parentEntry.Right;

	while (currentAddress) {
		IndexEntry entry = _cache[currentAddress];

		if (value == entry.Value) {
			entry.Valid = 0;
			_cache[currentAddress] = entry;
			Rollup(currentAddress);
			return;
		}

		if (value < entry.Value) {
			currentAddress = parentEntry.Left;
		} else {
			currentAddress = parentEntry.Right;
		}

		parentEntry = entry;
	}
}

void MessageStorageIndex::GetEntry(
	uint32_t address,
	int64_t &timestamp,
	int32_t &index,
	bool &incoming)
{
	IndexEntry entry = _cache[address];
	timestamp = entry.Value.Timestamp;
	index = entry.Value.Index;
	incoming = entry.Value.Incoming;
}

uint32_t MessageStorageIndex::FindSmallest(int64_t timestamp)
{
	IndexEntry entry = _cache[0];

	uint32_t smallestAddress = 0;
	int64_t smallestValue = 0;

	uint32_t address = entry.Right;

	while (address) {
		entry = _cache[address];

		if (timestamp > entry.Value.Timestamp) {
			address = entry.Right;
		} else {
			bool newSmallest = 
				!smallestAddress ||
				entry.Value.Timestamp < smallestValue;

			if (newSmallest) {
				smallestAddress = address;
				smallestValue = entry.Value.Timestamp;
			}

			address = entry.Left;
		}
	}

	return smallestAddress;
}

uint32_t MessageStorageIndex::Next(uint32_t address)
{
	IndexEntry entry = _cache[address];

	if (entry.Right) {
		address = entry.Right;
		entry = _cache[address];

		while (entry.Left) {
			address = entry.Left;
			entry = _cache[address];
		}

		return address;
	}

	for (;;) {
		uint32_t childAddress = address;
		address = entry.Parent;

		if (!address) {
			return 0;
		}

		entry = _cache[address];

		if (childAddress == entry.Left) {
			return address;
		}
	}
}

uint32_t MessageStorageIndex::Previous(uint32_t address)
{
	IndexEntry entry = _cache[address];

	if (entry.Left) {
		address = entry.Left;
		entry = _cache[address];

		while (entry.Right) {
			address = entry.Right;
			entry = _cache[address];
		}

		return address;
	}

	for (;;) {
		uint32_t childAddress = address;
		address = entry.Parent;

		if (!address) {
			return 0;
		}

		entry = _cache[address];

		if (childAddress == entry.Right) {
			return address;
		}
	}
}

uint32_t MessageStorageIndex::FindBiggest()
{
	IndexEntry entry = _cache[0];

	uint32_t address = entry.Right;

	while (address) {
		entry = _cache[address];

		if (!entry.Right) {
			return address;
		}

		address = entry.Right;
	}

	return 0;
}

void MessageStorageIndex::RotateLeft(uint32_t address)
{
	/*      1                 2
	 *     / \               / \
	 *    A   2    --->     1   C
	 *       / \           / \
	 *      B   C         A   B
	 *
	 * Input argument is the address of node 1.
	 */
	uint32_t address1 = address;
	IndexEntry node1 = _cache[address1];

	if (!node1.Right) {
		return;
	}

	uint32_t address2 = node1.Right;
	IndexEntry node2 = _cache[address2];

	uint32_t addressA = node1.Left;
	uint32_t addressB = node2.Left;
	uint32_t addressC = node2.Right;

	uint32_t depthA = -1;
	uint32_t depthB = -1;
	uint32_t depthC = -1;

	if (addressA) {
		depthA = _cache[addressA].operator IndexEntry().Depth;
	}

	if (addressB) {
		depthB = _cache[addressB].operator IndexEntry().Depth;
	}

	if (addressC) {
		depthC = _cache[addressC].operator IndexEntry().Depth;
	}

	uint32_t parentAddress = node1.Parent;
	IndexEntry parentNode = _cache[parentAddress];

	node1.Left = addressA;
	node1.Right = addressB;
	node1.Parent = address2;
	node1.Depth = (depthA > depthB ? depthA : depthB) + 1;
	_cache[address1] = node1;

	node2.Left = address1;
	node2.Right = addressC;
	node2.Parent = parentAddress;
	node2.Depth = (node1.Depth > depthC ? node1.Depth : depthC) + 1;
	_cache[address2] = node2;

	if (parentNode.Left == address1) {
		parentNode.Left = address2;
	} else if (parentNode.Right == address1) {
		parentNode.Right = address2;
	} else {
		THROW("Parent does not have child reference.");
	}

	_cache[parentAddress] = parentNode;

	if (addressB) {
		IndexEntry nodeB = _cache[addressB];
		nodeB.Parent = address1;
		_cache[addressB] = nodeB;
	}
}

void MessageStorageIndex::Rollup(uint32_t address)
{
	IndexEntry entry;

	while (address) {
		entry = _cache[address];

		if (!entry.Valid && !entry.Left && !entry.Right) {
			IndexEntry parent = _cache[entry.Parent];

			if (address == parent.Left) {
				parent.Left = 0;
			} else if (address == parent.Right) {
				parent.Right = 0;
			} else {
				THROW("Parent does not have child reference.");
			}

			_cache[entry.Parent] = parent;
			Free(address);

			address = parent.This;
			continue;
		}

		uint32_t leftDepth = 0;
		uint32_t rightDepth = 0;

		if (entry.Left) {
			IndexEntry left = _cache[entry.Left];
			leftDepth = left.Depth + 1;
		}

		if (entry.Right) {
			IndexEntry right = _cache[entry.Right];
			rightDepth = right.Depth + 1;
		}

		entry.Depth = leftDepth > rightDepth ? leftDepth : rightDepth;

		_cache[entry.This] = entry;

		if (rightDepth > leftDepth + 2) {
			RotateLeft(address);
			entry = _cache[address];
		} else {
			address = entry.Parent;
		}
	}
}

uint32_t MessageStorageIndex::Allocate()
{
	IndexEntry entry = _cache[0];

	if (!entry.Left) {
		return _file.Size() / sizeof(IndexEntry);
	}

	uint32_t newAddress = entry.Left;

	IndexEntry allocEntry = _cache[newAddress];

	entry.Left = allocEntry.Right;
	_cache[0] = entry;

	return newAddress;
}

void MessageStorageIndex::Free(uint32_t address)
{
	IndexEntry root = _cache[0];

	IndexEntry entry = _cache[address];
	entry.Valid = 0;
	entry.Right = root.Left;
	root.Left = entry.This;
	_cache[address] = entry;
	_cache[0] = root;
}

// Storage.
MessageStorage::MessageStorage(const uint8_t *ownerKey)
{
	_ownerKey = ownerKey;
}

MessageStorage::~MessageStorage()
{
}

void MessageStorage::GetFreeTimestampIndex(
	const uint8_t *peerKey,
	int64_t timestamp,
	int32_t &index)
{
	index = 0;

	String peerKeyHex = DataToHex(peerKey, KEY_SIZE);
	String ownerKeyHex = DataToHex(_ownerKey, KEY_SIZE);
	String timeString = ToHex(timestamp);

	String prefix = String("storage/") + ownerKeyHex + "/storage/" +
		peerKeyHex + "/out/";

	String path = prefix + timeString + "_" + ToHex(index);

	while (FileExists(path)) {
		++index;
		path = prefix + timeString + "_" + ToHex(index);
	}
}

bool MessageStorage::MessageExists(
	const uint8_t *peerKey,
	int64_t timestamp,
	int32_t index,
	bool incoming)
{
	String peerKeyHex = DataToHex(peerKey, KEY_SIZE);
	String ownerKeyHex = DataToHex(_ownerKey, KEY_SIZE);

	String path = String("storage/") + ownerKeyHex + "/storage/" +
		peerKeyHex + (incoming ? "/in/" : "/out/") +
		ToHex(timestamp) + "_" + ToHex(index);

	return FileExists(path);
}

bool MessageStorage::AddMessage(CowBuffer<uint8_t> message)
{
	int64_t timestamp;
	int32_t index;
	const uint8_t *peerKey = nullptr;

	memcpy(&timestamp, message.Pointer() + KEY_SIZE * 2, sizeof(timestamp));
	memcpy(
		&index,
		message.Pointer() + KEY_SIZE * 2 + sizeof(timestamp),
		sizeof(index));

	bool incoming;

	if (!crypto_verify32(_ownerKey, message.Pointer())) {
		incoming = false;
		peerKey = message.Pointer() + KEY_SIZE;
	} else {
		incoming = true;
		peerKey = message.Pointer();
	}

	String peerKeyHex = DataToHex(peerKey, KEY_SIZE);
	String ownerKeyHex = DataToHex(_ownerKey, KEY_SIZE);

	String entryPath = "storage";
	CreateDirectory(entryPath);
	entryPath += String("/") + ownerKeyHex;
	CreateDirectory(entryPath);
	entryPath += "/storage";
	CreateDirectory(entryPath);
	entryPath += String("/") + peerKeyHex;
	CreateDirectory(entryPath);
	entryPath += incoming ? "/in" : "/out";
	CreateDirectory(entryPath);

	entryPath += String("/") + ToHex(timestamp) + "_" + ToHex(index);

	if (FileExists(entryPath)) {
		return false;
	}

	BinaryFile file(entryPath, true);

	file.Write<uint8_t>(
		message.Pointer(),
		message.Size(),
		0);

	MessageStorageIndex storageIndex(
		String("storage/") + ownerKeyHex + "/storage/" +
		peerKeyHex + "/index");
	storageIndex.AddEntry(timestamp, index, incoming);

	return true;
}

CowBuffer<CowBuffer<uint8_t>> MessageStorage::GetMessageRange(
	int64_t from,
	int64_t to)
{
	struct Elem
	{
		Elem *Next;
		CowBuffer<uint8_t> Message;
	};

	String ownerKeyHex = DataToHex(_ownerKey, KEY_SIZE);

	Elem *first = nullptr;
	Elem **last = &first;

	int messageCount = 0;

	String path = String("storage/") + ownerKeyHex + "/storage";

	if (!FileExists(path)) {
		return CowBuffer<CowBuffer<uint8_t>>();
	}

	CowBuffer<String> peers = ListDirectory(path);

	for (uint32_t peerIdx = 0; peerIdx < peers.Size(); peerIdx++) {
		uint8_t peerKey[KEY_SIZE];
		HexToData(peers[peerIdx], peerKey);

		CowBuffer<CowBuffer<uint8_t>> peerMessages = GetMessageRange(
			peerKey,
			from,
			to);

		for (
			uint32_t msgIdx = 0;
			msgIdx < peerMessages.Size();
			msgIdx++)
		{
			Elem *elem = new Elem;
			elem->Next = nullptr;

			elem->Message = peerMessages[msgIdx];

			*last = elem;
			last = &((*last)->Next);

			++messageCount;
		}
	}

	if (!messageCount) {
		return CowBuffer<CowBuffer<uint8_t>>();
	}

	CowBuffer<CowBuffer<uint8_t>> result(messageCount);
	uint64_t index = 0;

	while (first) {
		result[index] = first->Message;
		++index;

		Elem *tmp = first;
		first = first->Next;
		delete tmp;
	}

	return result;
}

CowBuffer<CowBuffer<uint8_t>> MessageStorage::GetMessageRange(
	const uint8_t *peerKey,
	int64_t from,
	int64_t to)
{
	struct Elem
	{
		Elem *Next;
		CowBuffer<uint8_t> Message;
	};

	String ownerKeyHex = DataToHex(_ownerKey, KEY_SIZE);
	String peerKeyHex = DataToHex(peerKey, KEY_SIZE);

	Elem *first = nullptr;
	Elem **last = &first;

	int messageCount = 0;

	String path = String("storage/") + ownerKeyHex + "/storage/" +
		peerKeyHex;

	if (!FileExists(path)) {
		return CowBuffer<CowBuffer<uint8_t>>();
	}

	String indexPath = path + "/index";

	if (!FileExists(indexPath)) {
		return CowBuffer<CowBuffer<uint8_t>>();
	}

	MessageStorageIndex storageIndex(indexPath);

	uint32_t address = storageIndex.FindSmallest(from);

	while (address) {
		int64_t timestamp;
		int32_t index;
		bool incoming;

		storageIndex.GetEntry(address, timestamp, index, incoming);
		address = storageIndex.Next(address);

		if (timestamp > to) {
			break;
		}

		String entryPath = path + (incoming ? "/in/" : "/out/") +
			ToHex(timestamp) + "_" + ToHex(index);

		Elem *elem = new Elem;
		elem->Next = nullptr;

		BinaryFile file(entryPath, false);
		elem->Message = CowBuffer<uint8_t>(file.Size());
		file.Read<uint8_t>(
			elem->Message.Pointer(),
			elem->Message.Size(),
			0);

		*last = elem;
		last = &((*last)->Next);

		++messageCount;
	}

	if (!messageCount) {
		return CowBuffer<CowBuffer<uint8_t>>();
	}

	CowBuffer<CowBuffer<uint8_t>> result(messageCount);
	uint64_t index = 0;

	while (first) {
		result[index] = first->Message;
		++index;

		Elem *tmp = first;
		first = first->Next;
		delete tmp;
	}

	return result;
}

CowBuffer<CowBuffer<uint8_t>> MessageStorage::GetLatestNMessages(
	const uint8_t *peerKey,
	int requestedMessageCount)
{
	struct Elem
	{
		Elem *Next;
		CowBuffer<uint8_t> Message;
	};

	String ownerKeyHex = DataToHex(_ownerKey, KEY_SIZE);
	String peerKeyHex = DataToHex(peerKey, KEY_SIZE);

	Elem *first = nullptr;
	Elem **last = &first;

	int messageCount = 0;

	String path = String("storage/") + ownerKeyHex + "/storage/" +
		peerKeyHex;

	if (!FileExists(path)) {
		return CowBuffer<CowBuffer<uint8_t>>();
	}

	String indexPath = path + "/index";

	if (!FileExists(indexPath)) {
		return CowBuffer<CowBuffer<uint8_t>>();
	}

	MessageStorageIndex storageIndex(indexPath);
	uint32_t address = storageIndex.FindBiggest();

	while (address && messageCount < requestedMessageCount) {
		int64_t timestamp;
		int32_t index;
		bool incoming;

		storageIndex.GetEntry(address, timestamp, index, incoming);
		address = storageIndex.Previous(address);

		String entryPath = path + (incoming ? "/in/" : "/out/") +
			ToHex(timestamp) + "_" + ToHex(index);

		Elem *elem = new Elem;
		elem->Next = nullptr;

		BinaryFile file(entryPath, false);
		elem->Message = CowBuffer<uint8_t>(file.Size());
		file.Read<uint8_t>(
			elem->Message.Pointer(),
			elem->Message.Size(),
			0);

		*last = elem;
		last = &((*last)->Next);

		++messageCount;
	}

	if (!messageCount) {
		return CowBuffer<CowBuffer<uint8_t>>();
	}

	CowBuffer<CowBuffer<uint8_t>> result(messageCount);
	uint64_t index = 0;

	while (first) {
		result[index] = first->Message;
		++index;

		Elem *tmp = first;
		first = first->Next;
		delete tmp;
	}

	return result;
}
