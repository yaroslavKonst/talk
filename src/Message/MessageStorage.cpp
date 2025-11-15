#include "MessageStorage.hpp"

#include "../Common/Hex.hpp"
#include "../Common/BinaryFile.hpp"
#include "../Common/UnixTime.hpp"
#include "../Crypto/CryptoDefinitions.hpp"
#include "../ThirdParty/monocypher.h"

MessageStorage::MessageStorage(const uint8_t *ownerKey)
{
	_ownerKey = ownerKey;
}

MessageStorage::~MessageStorage()
{
}

void MessageStorage::GetFreeTimestampIndex(
	const uint8_t *peerKey,
	int64_t &timestamp,
	int32_t &index)
{
	timestamp = GetUnixTime();
	index = 0;

	String peerKeyHex = DataToHex(peerKey, KEY_SIZE);
	String ownerKeyHex = DataToHex(_ownerKey, KEY_SIZE);
	String timeString = ToHex(timestamp);

	String path = ownerKeyHex + "/storage/" + peerKeyHex + "/" +
		timeString + "_" + ToHex(index) + "_s";

	while (FileExists(path)) {
		++index;
		path = ownerKeyHex + "/storage/" + peerKeyHex + "/" +
			timeString + "_" + ToHex(index) + "_s";
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

	String path = ownerKeyHex + "/storage/" + peerKeyHex + "/" +
		ToHex(timestamp) + "_" + ToHex(index) +
		(incoming ? "_r" : "_s");

	return FileExists(path);
}

void MessageStorage::AddMessage(CowBuffer<uint8_t> message)
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

	String entryPath = ownerKeyHex;

	CreateDirectory(entryPath);
	entryPath += "/storage";
	CreateDirectory(entryPath);
	entryPath += String("/") + peerKeyHex;
	CreateDirectory(entryPath);

	entryPath += String("/") + ToHex(timestamp) + "_" + ToHex(index) +
		(incoming ? "_r" : "_s");

	if (FileExists(entryPath)) {
		THROW("Added message already exists.");
	}

	BinaryFile file(entryPath, true);

	file.Write<uint8_t>(
		message.Pointer(),
		message.Size(),
		0);
}

CowBuffer<uint8_t> *MessageStorage::GetMessageRange(
	int64_t from,
	int64_t to,
	uint64_t &messageCount)
{
	struct Elem
	{
		Elem *Next;
		CowBuffer<uint8_t> Message;
	};

	String ownerKeyHex = DataToHex(_ownerKey, KEY_SIZE);

	Elem *first = nullptr;
	Elem **last = &first;

	messageCount = 0;

	String path = ownerKeyHex + "/storage";

	if (!FileExists(path)) {
		return nullptr;
	}

	CowBuffer<String> peers = ListDirectory(path);

	for (uint32_t peerIdx = 0; peerIdx < peers.Size(); peerIdx++) {
		String peerPath = path + "/" + peers[peerIdx];

		CowBuffer<String> messageFiles = ListDirectory(peerPath);

		for (
			uint32_t msgIdx = 0;
			msgIdx < messageFiles.Size();
			msgIdx++)
		{
			String name = messageFiles[msgIdx];

			int partCount;
			String *parts = name.Split('_', false, partCount);

			if (partCount != 3) {
				THROW("Invalid message name.");
			}

			int64_t timestamp = HexToInt<int64_t>(parts[0].CStr());
			delete[] parts;

			if (timestamp >= from && timestamp <= to) {
				Elem *elem = new Elem;
				elem->Next = nullptr;

				BinaryFile file(peerPath + "/" + name, false);
				elem->Message = CowBuffer<uint8_t>(file.Size());
				file.Read<uint8_t>(
					elem->Message.Pointer(),
					elem->Message.Size(),
					0);

				*last = elem;
				last = &((*last)->Next);

				++messageCount;
			}
		}
	}

	if (!messageCount) {
		return nullptr;
	}

	CowBuffer<uint8_t> *result = new CowBuffer<uint8_t>[messageCount];
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

CowBuffer<uint8_t> *MessageStorage::GetMessageRange(
	const uint8_t *peerKey,
	int64_t from,
	int64_t to,
	uint64_t &messageCount)
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

	messageCount = 0;

	String path = ownerKeyHex + "/storage/" + peerKeyHex;

	if (!FileExists(path)) {
		return nullptr;
	}

	CowBuffer<String> messageFiles = ListDirectory(path);

	for (uint32_t msgIdx = 0; msgIdx < messageFiles.Size(); msgIdx++) {
		String name = messageFiles[msgIdx];

		int partCount;
		String *parts = name.Split('_', false, partCount);

		if (partCount != 3) {
			THROW("Invalid message name.");
		}

		int64_t timestamp = HexToInt<int64_t>(parts[0].CStr());
		delete[] parts;

		if (timestamp >= from && timestamp <= to) {
			Elem *elem = new Elem;
			elem->Next = nullptr;

			BinaryFile file(path + "/" + name, false);
			elem->Message = CowBuffer<uint8_t>(file.Size());
			file.Read<uint8_t>(
				elem->Message.Pointer(),
				elem->Message.Size(),
				0);

			*last = elem;
			last = &((*last)->Next);

			++messageCount;
		}
	}

	if (!messageCount) {
		return nullptr;
	}

	CowBuffer<uint8_t> *result = new CowBuffer<uint8_t>[messageCount];
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
