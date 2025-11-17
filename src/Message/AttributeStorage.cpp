#include "AttributeStorage.hpp"

#include "../Common/Hex.hpp"
#include "../Common/BinaryFile.hpp"
#include "../Common/UnixTime.hpp"
#include "../Crypto/CryptoDefinitions.hpp"
#include "../ThirdParty/monocypher.h"

AttributeStorage::AttributeStorage(const uint8_t *ownerKey)
{
	_ownerKey = ownerKey;
}

AttributeStorage::~AttributeStorage()
{
}

void AttributeStorage::SetAttribute(
	CowBuffer<uint8_t> message,
	uint32_t attribute)
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
	entryPath += "/attributes";
	CreateDirectory(entryPath);
	entryPath += String("/") + peerKeyHex;
	CreateDirectory(entryPath);

	entryPath += String("/") + ToHex(timestamp) + "_" + ToHex(index) +
		(incoming ? "_r" : "_s");

	if (!attribute) {
		if (FileExists(entryPath)) {
			int res = unlink(entryPath.CStr());

			if (res == -1) {
				THROW("Failed to unlink attribute file.");
			}
		}

		return;
	}

	BinaryFile file(entryPath, true);
	file.Write<uint32_t>(&attribute, 1, 0);
}

uint32_t AttributeStorage::GetAttribute(CowBuffer<uint8_t> message)
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
	entryPath += String("/") + ownerKeyHex;
	entryPath += "/attributes";
	entryPath += String("/") + peerKeyHex;

	entryPath += String("/") + ToHex(timestamp) + "_" + ToHex(index) +
		(incoming ? "_r" : "_s");

	if (!FileExists(entryPath)) {
		return 0;
	}

	uint32_t attribute;

	BinaryFile file(entryPath, false);
	file.Read<uint32_t>(&attribute, 1, 0);
	return attribute;
}
