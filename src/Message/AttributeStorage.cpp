#include "AttributeStorage.hpp"

#include <cstring>

#include "Message.hpp"
#include "../Common/Hex.hpp"
#include "../Common/BinaryFile.hpp"
#include "../Common/File.hpp"
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
	String ownerKeyHex = DataToHex(_ownerKey, KEY_SIZE);

	String entryPath = "storage";
	CreateDirectory(entryPath);
	entryPath += String("/") + ownerKeyHex;
	CreateDirectory(entryPath);
	entryPath += "/attributes";
	CreateDirectory(entryPath);
	entryPath += String("/") + peerKeyHex;
	CreateDirectory(entryPath);

	entryPath += String("/") +
		ToHex(header.Timestamp) + "_" + ToHex(header.Index) +
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
	String ownerKeyHex = DataToHex(_ownerKey, KEY_SIZE);

	String entryPath = "storage";
	entryPath += String("/") + ownerKeyHex;
	entryPath += "/attributes";
	entryPath += String("/") + peerKeyHex;

	entryPath += String("/") +
		ToHex(header.Timestamp) + "_" + ToHex(header.Index) +
		(incoming ? "_r" : "_s");

	if (!FileExists(entryPath)) {
		return 0;
	}

	uint32_t attribute;

	BinaryFile file(entryPath, false);
	file.Read<uint32_t>(&attribute, 1, 0);
	return attribute;
}
