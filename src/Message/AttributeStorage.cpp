#include "AttributeStorage.hpp"

#include <cstring>

#include "Message.hpp"
#include "../Common/Hex.hpp"
#include "../Common/BinaryFile.hpp"
#include "../Common/File.hpp"
#include "../Common/FileTree.hpp"
#include "../Common/UnixTime.hpp"
#include "../Crypto/CryptoDefinitions.hpp"
#include "../ThirdParty/monocypher.h"

AttributeStorage::AttributeStorage(const uint8_t *ownerKey)
{
	_ownerKey = ownerKey;

	String ownerKeyHex = DataToHex(_ownerKey, KEY_SIZE);
	_rootPath + "storage/" + ownerKeyHex + "/attributes";

	if (!FileExists(_rootPath)) {
		CreateDirectory("storage");
		CreateDirectory("storage/" + ownerKeyHex);
		CreateDirectory(_rootPath);
	}
}

AttributeStorage::~AttributeStorage()
{
}

struct AttributeIndexEntry
{
	Message::MessageID ID;
	uint32_t Attribute;

	AttributeIndexEntry()
	{ }

	AttributeIndexEntry(const Message::MessageID &id, uint32_t attr = 0)
	{
		ID = id;
		Attribute = attr;
	}

	bool operator==(const AttributeIndexEntry &entry) const
	{
		return ID == entry.ID;
	}

	bool operator<(const AttributeIndexEntry &entry) const
	{
		return ID < entry.ID;
	}
};

void AttributeStorage::SetAttribute(
	const Message::MessageID &id,
	uint32_t attribute)
{
	const uint8_t *peerKey = nullptr;

	if (!crypto_verify32(_ownerKey, id.Source)) {
		peerKey = id.Destination;
	} else {
		peerKey = id.Source;
	}

	String peerKeyHex = DataToHex(peerKey, KEY_SIZE);
	String indexPath = _rootPath + "/" + peerKeyHex;

	FileTree<AttributeIndexEntry> storage(indexPath);
	uint32_t entryAddress = storage.FindEntry(id);

	if (!attribute) {
		if (!entryAddress) {
			return;
		}

		storage.RemoveEntry(entryAddress);
		return;
	}

	if (!entryAddress) {
		storage.AddEntry(AttributeIndexEntry(id, attribute));
		return;
	}

	AttributeIndexEntry entry = storage.GetEntry(entryAddress);
	entry.Attribute = attribute;
	storage.SetEntry(entryAddress, entry);
}

uint32_t AttributeStorage::GetAttribute(const Message::MessageID &id)
{
	const uint8_t *peerKey = nullptr;

	if (!crypto_verify32(_ownerKey, id.Source)) {
		peerKey = id.Destination;
	} else {
		peerKey = id.Source;
	}

	String peerKeyHex = DataToHex(peerKey, KEY_SIZE);
	String indexPath = _rootPath + "/" + peerKeyHex;

	FileTree<AttributeIndexEntry> storage(indexPath);
	uint32_t entryAddress = storage.FindEntry(id);

	if (!entryAddress) {
		return 0;
	}

	AttributeIndexEntry entry = storage.GetEntry(entryAddress);
	return entry.Attribute;
}

CowBuffer<Message::MessageID> AttributeStorage::ListUnsent()
{
	struct Entry
	{
		Message::MessageID ID;
		Entry *Next;
	};

	Entry *first = nullptr;
	Entry **last = &first;

	int messageCount = 0;

	CowBuffer<String> peerKeyStrings = ListDirectory(_rootPath);

	for (int peerIdx = 0; peerIdx < (int)peerKeyStrings.Size(); peerIdx++) {
		FileTree<AttributeIndexEntry> storage(
			_rootPath + "/" + peerKeyStrings[peerIdx]);

		uint32_t address = storage.FindSmallest();

		while (address) {
			AttributeIndexEntry entry = storage.GetEntry(address);
			address = storage.Next(address);

			if (!(entry.Attribute & ATTRIBUTE_SENT)) {
				continue;
			}

			Entry *listEntry = new Entry;
			listEntry->Next = nullptr;
			listEntry->ID = entry.ID;

			*last = listEntry;
			last = &listEntry->Next;

			++messageCount;
		}
	}

	if (!messageCount) {
		return CowBuffer<Message::MessageID>();
	}

	CowBuffer<Message::MessageID> result(messageCount);
	int index = 0;

	while (first) {
		result[index] = first->ID;
		++index;

		Entry *tmp = first;
		first = first->Next;
		delete tmp;
	}

	return result;
}
