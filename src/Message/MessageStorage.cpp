#include "MessageStorage.hpp"

#include "Message.hpp"
#include "../Common/Hex.hpp"
#include "../Common/BinaryFile.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/File.hpp"
#include "../Crypto/CryptoDefinitions.hpp"
#include "../ThirdParty/monocypher.h"

MessageStorage::MessageStorage(const uint8_t *ownerKey)
{
	_ownerKey = ownerKey;

	String ownerKeyHex = DataToHex(ownerKey, KEY_SIZE);
	_rootPath = "storage/" + ownerKeyHex + "/storage";

	if (!FileExists(_rootPath)) {
		CreateDirectory("storage");
		CreateDirectory("storage/" + ownerKeyHex);
		CreateDirectory(_rootPath);
	}
}

MessageStorage::~MessageStorage()
{
}

Message::MessageID MessageStorage::GetFreeHeader(
	const uint8_t *peerKey,
	int64_t timestamp)
{
	String peerKeyHex = DataToHex(peerKey, KEY_SIZE);
	String timeString = ToHex(timestamp);

	String prefix = _rootPath + peerKeyHex + "/out/";

	int32_t index = -1;
	String path;

	do {
		++index;
		path = prefix + timeString + "_" + ToHex(index);
	} while (FileExists(path));

	Message::MessageID id;
	id.Timestamp = timestamp;
	id.Index = index;
	memcpy(id.Source, _ownerKey, KEY_SIZE);
	memcpy(id.Destination, peerKey, KEY_SIZE);

	return id;
}

bool MessageStorage::MessageExists(const Message::MessageID &id)
{
	const uint8_t *peerKey;
	bool incoming;

	if (!crypto_verify32(_ownerKey, id.Source)) {
		peerKey = id.Destination;
		incoming = false;
	} else {
		peerKey = id.Source;
		incoming = true;
	}

	String peerKeyHex = DataToHex(peerKey, KEY_SIZE);

	String path = _rootPath + peerKeyHex +
		(incoming ? "/in/" : "/out/") +
		ToHex(id.Timestamp) + "_" + ToHex(id.Index);

	return FileExists(path);
}

bool MessageStorage::AddMessage(CowBuffer<uint8_t> message)
{
	Message::Header header;
	bool res = Message::GetHeader(message, header);

	if (!res) {
		THROW("Invalid message header.");
	}

	const uint8_t *peerKey = nullptr;
	bool incoming;

	if (!crypto_verify32(_ownerKey, header.Source)) {
		incoming = false;
		peerKey = header.Destination;
	} else {
		incoming = true;
		peerKey = header.Source;
	}

	String peerKeyHex = DataToHex(peerKey, KEY_SIZE);

	String entryPath = _rootPath + peerKeyHex;
	CreateDirectory(entryPath);
	entryPath += incoming ? "/in" : "/out";
	CreateDirectory(entryPath);

	entryPath += "/" + ToHex(header.Timestamp) + "_" +
		ToHex(header.Index);

	if (FileExists(entryPath)) {
		return false;
	}

	BinaryFile file(entryPath, true);

	file.Write<uint8_t>(
		message.Pointer(),
		message.Size(),
		0);

	Message::MessageID id;
	bool gotId = GetID(message, id);

	if (!gotId) {
		THROW("Invalid message header.");
	}

	FileTree<Message::MessageID> storageIndex(
		_rootPath + peerKeyHex + "/index");

	storageIndex.AddEntry(id);

	return true;
}

CowBuffer<uint8_t> MessageStorage::GetMessage(const Message::MessageID &id)
{
	const uint8_t *peerKey;
	bool incoming;

	if (!crypto_verify32(_ownerKey, id.Source)) {
		peerKey = id.Destination;
		incoming = false;
	} else {
		peerKey = id.Source;
		incoming = true;
	}

	String peerKeyHex = DataToHex(peerKey, KEY_SIZE);

	String path = _rootPath + peerKeyHex +
		(incoming ? "/in/" : "/out/") +
		ToHex(id.Timestamp) + "_" + ToHex(id.Index);

	BinaryFile file(path, false);
	CowBuffer<uint8_t> message(file.Size());

	file.Read<uint8_t>(
		message.Pointer(),
		message.Size(),
		0);

	return message;
}

CowBuffer<Message::MessageID> MessageStorage::GetMessageRange(
	int64_t from,
	int64_t to)
{
	struct Elem
	{
		Elem *Next;
		Message::MessageID ID;
	};

	Elem *first = nullptr;
	Elem **last = &first;

	int messageCount = 0;

	String path = _rootPath;

	if (!FileExists(path)) {
		return CowBuffer<Message::MessageID>();
	}

	CowBuffer<String> peers = ListDirectory(path);

	for (uint32_t peerIdx = 0; peerIdx < peers.Size(); peerIdx++) {
		uint8_t peerKey[KEY_SIZE];
		HexToData(peers[peerIdx], peerKey);

		CowBuffer<Message::MessageID> peerMessages = GetMessageRange(
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

			elem->ID = peerMessages[msgIdx];

			*last = elem;
			last = &((*last)->Next);

			++messageCount;
		}
	}

	if (!messageCount) {
		return CowBuffer<Message::MessageID>();
	}

	CowBuffer<Message::MessageID> result(messageCount);
	uint64_t index = 0;

	while (first) {
		result[index] = first->ID;
		++index;

		Elem *tmp = first;
		first = first->Next;
		delete tmp;
	}

	return result;
}

CowBuffer<Message::MessageID> MessageStorage::GetMessageRange(
	const uint8_t *peerKey,
	int64_t from,
	int64_t to)
{
	struct Elem
	{
		Elem *Next;
		Message::MessageID ID;
	};

	String peerKeyHex = DataToHex(peerKey, KEY_SIZE);

	Elem *first = nullptr;
	Elem **last = &first;

	int messageCount = 0;

	String path = _rootPath + peerKeyHex;

	if (!FileExists(path)) {
		return CowBuffer<Message::MessageID>();
	}

	String indexPath = path + "/index";

	if (!FileExists(indexPath)) {
		return CowBuffer<Message::MessageID>();
	}

	FileTree<Message::MessageID> storageIndex(indexPath);

	Message::MessageID firstID;
	memset(&firstID, 0, sizeof(firstID));
	firstID.Timestamp = from;

	Message::MessageID lastID;
	memset(&lastID, 0, sizeof(lastID));
	lastID.Timestamp = to;

	uint32_t address = storageIndex.FindSmallest(firstID);

	while (address) {
		Message::MessageID currentID = storageIndex.GetEntry(address);
		address = storageIndex.Next(address);

		if (!(currentID < lastID)) {
			break;
		}


		Elem *elem = new Elem;
		elem->Next = nullptr;
		elem->ID = currentID;

		*last = elem;
		last = &((*last)->Next);

		++messageCount;
	}

	if (!messageCount) {
		return CowBuffer<Message::MessageID>();
	}

	CowBuffer<Message::MessageID> result(messageCount);
	uint64_t index = 0;

	while (first) {
		result[index] = first->ID;
		++index;

		Elem *tmp = first;
		first = first->Next;
		delete tmp;
	}

	return result;
}
