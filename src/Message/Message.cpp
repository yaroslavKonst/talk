#include "Message.hpp"

#include <cstring>

#include "../Protocol/ParserHelpers.hpp"
#include "../Crypto/Crypto.hpp"
#include "../Common/Exception.hpp"
#include "../ThirdParty/monocypher.h"

using namespace Crypto::X25519;

const int32_t MaxNameLength = 500;

static uint64_t EntryDataSize(const Message::ContentsEntry *entry)
{
	switch (entry->Type) {
	case Message::ContentsEntryType::Text:
	{
		const Message::ContentsEntryText *e =
			static_cast<const Message::ContentsEntryText*>(entry);
		return e->Text.Length();
	}

	case Message::ContentsEntryType::Attachment:
	{
		const Message::ContentsEntryAttachment *e =
			static_cast<const Message::ContentsEntryAttachment*>(
				entry);
		return BuiltStringSize(e->AttachmentName) +
			e->Attachment.Size();
	}

	case Message::ContentsEntryType::Key:
	{
		const Message::ContentsEntryKey *e =
			static_cast<const Message::ContentsEntryKey*>(entry);
		return BuiltStringSize(e->UserName) + sizeof(int32_t) +
			KEY_SIZE;
	}

	default:
		return 0;
	}
}

static void WriteEntryData(
	CowBuffer<uint8_t> &buffer,
	uint64_t &offset,
	const Message::ContentsEntry *entry)
{
	switch (entry->Type) {
	case Message::ContentsEntryType::Text:
	{
		const Message::ContentsEntryText *e =
			static_cast<const Message::ContentsEntryText*>(entry);
		memcpy(
			buffer.Pointer(offset),
			e->Text.CStr(),
			e->Text.Length());
		offset += e->Text.Length();
		break;
	}

	case Message::ContentsEntryType::Attachment:
	{
		const Message::ContentsEntryAttachment *e =
			static_cast<const Message::ContentsEntryAttachment*>(
				entry);
		BuildString(buffer, offset, e->AttachmentName);
		memcpy(
			buffer.Pointer(offset),
			e->Attachment.Pointer(),
			e->Attachment.Size());
		offset += e->Attachment.Size();
		break;
	}

	case Message::ContentsEntryType::Key:
	{
		const Message::ContentsEntryKey *e =
			static_cast<const Message::ContentsEntryKey*>(entry);
		BuildString(buffer, offset, e->UserName);
		*buffer.SwitchType<int32_t>(offset) = e->Key.KeyType;
		offset += sizeof(int32_t);
		memcpy(buffer.Pointer(offset), e->Key.Key, KEY_SIZE);
		offset += KEY_SIZE;
		break;
	}

	default:
		break;
	}
}

Message::Type Message::GetMessageType(const CowBuffer<uint8_t> message)
{
	if (!message.Size()) {
		THROW("Empty message.");
	}

	switch ((Type)message[0]) {
	case Type::PointToPoint:
		return Type::PointToPoint;
	case Type::Group:
		return Type::Group;
	default:
		THROW("Unknown message type.");
	}
}

Message::Contents::~Contents()
{
	for (uint64_t i = 0; i < Entries.Size(); i++) {
		delete Entries[i];
		Entries[i] = nullptr;
	}
}

bool Message::ParseContents(
	const CowBuffer<uint8_t> message,
	Contents &contents)
{
	struct Node
	{
		Node *Next;
		ContentsEntry *Entry;
	};

	Node *first = nullptr;
	Node *last = nullptr;
	uint64_t count = 0;
	bool failed = false;

	uint64_t offset = 0;

	while (offset < message.Size()) {
		if (message.Size() <
			offset + sizeof(uint8_t) + sizeof(uint64_t))
		{
			failed = true;
			break;
		}

		uint8_t entryTypeRaw = message[offset];
		offset += 1;

		uint64_t dataSize = *message.SwitchType<uint64_t>(offset);
		offset += sizeof(uint64_t);

		if (message.Size() < offset + dataSize) {
			failed = true;
			break;
		}

		const CowBuffer<uint8_t> data = message.Slice(offset, dataSize);
		offset += dataSize;

		ContentsEntry *entry = nullptr;

		switch ((ContentsEntryType)entryTypeRaw) {
		case ContentsEntryType::Text:
		{
			ContentsEntryText *e = new ContentsEntryText();
			e->Type = ContentsEntryType::Text;
			e->Text = String(
				data.SwitchType<char>(),
				(int)dataSize);
			entry = e;
			break;
		}

		case ContentsEntryType::Attachment:
		{
			uint64_t dataOffset = 0;
			String name;

			if (!ParseString(data, dataOffset, name)) {
				failed = true;
				break;
			}

			ContentsEntryAttachment *e =
				new ContentsEntryAttachment();
			e->Type = ContentsEntryType::Attachment;
			e->AttachmentName = name;
			e->Attachment = data.Slice(
				dataOffset,
				data.Size() - dataOffset);
			entry = e;
			break;
		}

		case ContentsEntryType::Key:
		{
			uint64_t dataOffset = 0;
			String userName;

			if (!ParseString(
				data,
				dataOffset,
				userName,
				MaxNameLength))
			{
				failed = true;
				break;
			}

			if (data.Size() <
				dataOffset + sizeof(int32_t) + KEY_SIZE)
			{
				failed = true;
				break;
			}

			ContentsEntryKey *e = new ContentsEntryKey();
			e->Type = ContentsEntryType::Key;
			e->UserName = userName;

			dataOffset += sizeof(int32_t);

			memcpy(
				e->Key.Key,
				data.Pointer(dataOffset),
				KEY_SIZE);
			entry = e;
			break;
		}

		case ContentsEntryType::Empty:
			// Empty entry, skip.
			break;

		default:
			ContentsEntryUnsupported *e =
				new ContentsEntryUnsupported();
			e->Type = ContentsEntryType::Unsupported;
			e->EntryTypeID = entryTypeRaw;
			entry = e;
			break;
		}

		if (failed) {
			break;
		}

		if (entry) {
			Node *node = new Node;
			node->Next = nullptr;
			node->Entry = entry;

			if (last) {
				last->Next = node;
			} else {
				first = node;
			}

			last = node;
			count += 1;
		}
	}

	if (failed) {
		while (first) {
			Node *tmp = first;
			first = first->Next;
			delete tmp->Entry;
			delete tmp;
		}

		return false;
	}

	contents.Entries.Resize(count);

	uint64_t index = 0;

	while (first) {
		contents.Entries[index] = first->Entry;
		index += 1;

		Node *tmp = first;
		first = first->Next;
		delete tmp;
	}

	return true;
}

CowBuffer<uint8_t> Message::BuildContents(
	const Contents &contents,
	uint64_t emptySize)
{
	uint64_t size = 0;

	for (uint64_t i = 0; i < contents.Entries.Size(); i++) {
		size += sizeof(uint8_t) + sizeof(uint64_t) +
			EntryDataSize(contents.Entries[i]);
	}

	if (emptySize) {
		size += sizeof(uint8_t) + sizeof(uint64_t) + emptySize;
	}

	CowBuffer<uint8_t> buffer(size);
	uint64_t offset = 0;

	for (uint64_t i = 0; i < contents.Entries.Size(); i++) {
		const ContentsEntry *entry = contents.Entries[i];
		uint64_t dataSize = EntryDataSize(entry);

		buffer[offset] = (uint8_t)entry->Type;
		offset += 1;

		*buffer.SwitchType<uint64_t>(offset) = dataSize;
		offset += sizeof(uint64_t);

		WriteEntryData(buffer, offset, entry);
	}

	if (emptySize) {
		buffer[offset] = (uint8_t)ContentsEntryType::Empty;
		offset += 1;

		*buffer.SwitchType<uint64_t>(offset) = emptySize;
		offset += sizeof(uint64_t);

		Crypto::GenerateRandomData(
			emptySize,
			buffer.Pointer(offset));
		offset += emptySize;
	}

	return buffer;
}

bool Message::X25519::ParseHeader(
	const CowBuffer<uint8_t> message,
	HeaderPointToPoint &header)
{
	// Message type.
	if (message.Size() < 1) {
		return false;
	}

	if ((Type)message[0] != Type::PointToPoint) {
		return false;
	}

	uint64_t offset = 1;

	// Encryption scheme.
	if (message.Size() < offset + sizeof(int32_t)) {
		return false;
	}

	if (*message.SwitchType<int32_t>(offset) != SCHEME_ID) {
		return false;
	}

	offset += sizeof(int32_t);

	// Source address.
	if (!ParseString(message, offset, header.Source, MaxNameLength)) {
		return false;
	}

	if (!header.Source.Length()) {
		return false;
	}

	// Source key.
	if (message.Size() < offset + KEY_SIZE) {
		return false;
	}

	memcpy(header.SourceKey.Key, message.Pointer(offset), KEY_SIZE);
	offset += KEY_SIZE;

	// Destination address.
	if (!ParseString(message, offset, header.Destination, MaxNameLength)) {
		return false;
	}

	if (!header.Destination.Length()) {
		return false;
	}

	// Destination key.
	if (message.Size() < offset + KEY_SIZE) {
		return false;
	}

	memcpy(header.DestinationKey.Key, message.Pointer(offset), KEY_SIZE);
	offset += KEY_SIZE;

	// Timestamp.
	if (message.Size() < offset + sizeof(header.Timestamp)) {
		return false;
	}

	header.Timestamp = *message.SwitchType<int64_t>(offset);
	offset += sizeof(header.Timestamp);

	// Index.
	if (message.Size() < offset + sizeof(header.Index)) {
		return false;
	}

	header.Index = *message.SwitchType<int32_t>(offset);
	offset += sizeof(header.Index);

	// Thread ID.
	if (message.Size() < offset +
		(uint64_t)ObjectStorage::Constants::IDSize)
	{
		return false;
	}

	header.ThreadID.SetValue(message.Pointer(offset));
	offset += (uint64_t)ObjectStorage::Constants::IDSize;

	// Message size.
	if (message.Size() < offset + sizeof(header.MessageSize)) {
		return false;
	}

	header.MessageSize = *message.SwitchType<uint64_t>(offset);
	offset += sizeof(header.MessageSize);

	// Nonce.
	if (message.Size() < offset + NONCE_SIZE) {
		return false;
	}

	memcpy(header.Nonce, message.Pointer(offset), NONCE_SIZE);
	offset += NONCE_SIZE;

	// Write header size.
	header.HeaderSize = offset;

	return true;
}

CowBuffer<uint8_t> Message::X25519::BuildHeader(
	const HeaderPointToPoint &header)
{
	CowBuffer<uint8_t> buffer(header.HeaderSize);

	buffer[0] = (uint8_t)Type::PointToPoint;
	uint64_t offset = 1;

	*buffer.SwitchType<int32_t>(offset) = SCHEME_ID;
	offset += sizeof(int32_t);

	BuildString(buffer, offset, header.Source);

	memcpy(buffer.Pointer(offset), header.SourceKey.Key, KEY_SIZE);
	offset += KEY_SIZE;

	BuildString(buffer, offset, header.Destination);

	memcpy(buffer.Pointer(offset), header.DestinationKey.Key, KEY_SIZE);
	offset += KEY_SIZE;

	*buffer.SwitchType<int64_t>(offset) = header.Timestamp;
	offset += sizeof(header.Timestamp);

	*buffer.SwitchType<int32_t>(offset) = header.Index;
	offset += sizeof(header.Index);

	memcpy(
		buffer.Pointer(offset),
		header.ThreadID.GetValue(),
		(uint64_t)ObjectStorage::Constants::IDSize);
	offset += (uint64_t)ObjectStorage::Constants::IDSize;

	*buffer.SwitchType<uint64_t>(offset) = header.MessageSize;
	offset += sizeof(header.MessageSize);

	memcpy(buffer.Pointer(offset), header.Nonce, NONCE_SIZE);
	offset += NONCE_SIZE;

	return buffer;
}

void Message::X25519::WriteHeaderSize(HeaderPointToPoint &header)
{
	header.HeaderSize =
		1 +
		sizeof(int32_t) +
		BuiltStringSize(header.Source) +
		KEY_SIZE +
		BuiltStringSize(header.Destination) +
		KEY_SIZE +
		sizeof(header.Timestamp) +
		sizeof(header.Index) +
		(uint64_t)ObjectStorage::Constants::IDSize +
		sizeof(header.MessageSize) +
		NONCE_SIZE;
}

bool Message::X25519::ParseHeader(
	const CowBuffer<uint8_t> message,
	HeaderGroup &header)
{
	// Message type.
	if (message.Size() < 1) {
		return false;
	}

	if ((Type)message[0] != Type::Group) {
		return false;
	}

	uint64_t offset = 1;

	// Encryption scheme.
	if (message.Size() < offset + sizeof(int32_t)) {
		return false;
	}

	if (*message.SwitchType<int32_t>(offset) != SCHEME_ID) {
		return false;
	}

	offset += sizeof(int32_t);

	// Source address.
	if (!ParseString(message, offset, header.Source, MaxNameLength)) {
		return false;
	}

	if (!header.Source.Length()) {
		return false;
	}

	// Source key.
	if (message.Size() < offset + KEY_SIZE) {
		return false;
	}

	memcpy(header.SourceKey.Key, message.Pointer(offset), KEY_SIZE);
	offset += KEY_SIZE;

	// Group name.
	if (!ParseString(message, offset, header.GroupName, MaxNameLength)) {
		return false;
	}

	if (!header.GroupName.Length()) {
		return false;
	}

	// Group keys.
	if (message.Size() < offset + sizeof(int32_t)) {
		return false;
	}

	int32_t keyCount = *message.SwitchType<int32_t>(offset);
	offset += sizeof(int32_t);

	if (keyCount < 0) {
		return false;
	}

	uint64_t keysSize = (uint64_t)keyCount * (uint64_t)GroupKeyEntrySize;

	if (message.Size() < offset + keysSize) {
		return false;
	}

	header.GroupKeys.Resize(keyCount);

	for (int32_t i = 0; i < keyCount; i++) {
		uint64_t entryOffset =
			offset + (uint64_t)i * GroupKeyEntrySize;

		GroupKeyEntry &entry = header.GroupKeys[i];

		memcpy(
			entry.DestinationKey.Key,
			message.Pointer(entryOffset),
			KEY_SIZE);
		entryOffset += KEY_SIZE;

		memcpy(entry.MAC, message.Pointer(entryOffset), MAC_SIZE);
		entryOffset += MAC_SIZE;

		memcpy(entry.Nonce, message.Pointer(entryOffset), NONCE_SIZE);
		entryOffset += NONCE_SIZE;

		memcpy(
			entry.EncryptedKey,
			message.Pointer(entryOffset),
			KEY_SIZE);
	}

	offset += keysSize;

	// Timestamp.
	if (message.Size() < offset + sizeof(header.Timestamp)) {
		return false;
	}

	header.Timestamp = *message.SwitchType<int64_t>(offset);
	offset += sizeof(header.Timestamp);

	// Index.
	if (message.Size() < offset + sizeof(header.Index)) {
		return false;
	}

	header.Index = *message.SwitchType<int32_t>(offset);
	offset += sizeof(header.Index);

	// Thread ID.
	if (message.Size() < offset +
		(uint64_t)ObjectStorage::Constants::IDSize)
	{
		return false;
	}

	header.ThreadID.SetValue(message.Pointer(offset));
	offset += (uint64_t)ObjectStorage::Constants::IDSize;

	// Message size.
	if (message.Size() < offset + sizeof(header.MessageSize)) {
		return false;
	}

	header.MessageSize = *message.SwitchType<uint64_t>(offset);
	offset += sizeof(header.MessageSize);

	// Nonce.
	if (message.Size() < offset + NONCE_SIZE) {
		return false;
	}

	memcpy(header.Nonce, message.Pointer(offset), NONCE_SIZE);
	offset += NONCE_SIZE;

	// Write header size.
	header.HeaderSize = offset;

	return true;
}

CowBuffer<uint8_t> Message::X25519::BuildHeader(const HeaderGroup &header)
{
	CowBuffer<uint8_t> buffer(header.HeaderSize);

	buffer[0] = (uint8_t)Type::Group;
	uint64_t offset = 1;

	*buffer.SwitchType<int32_t>(offset) = SCHEME_ID;
	offset += sizeof(int32_t);

	BuildString(buffer, offset, header.Source);

	memcpy(buffer.Pointer(offset), header.SourceKey.Key, KEY_SIZE);
	offset += KEY_SIZE;

	BuildString(buffer, offset, header.GroupName);

	*buffer.SwitchType<int32_t>(offset) = (int32_t)header.GroupKeys.Size();
	offset += sizeof(int32_t);

	for (uint64_t i = 0; i < header.GroupKeys.Size(); i++) {
		const GroupKeyEntry &entry = header.GroupKeys[i];

		memcpy(
			buffer.Pointer(offset),
			entry.DestinationKey.Key,
			KEY_SIZE);
		offset += KEY_SIZE;

		memcpy(buffer.Pointer(offset), entry.MAC, MAC_SIZE);
		offset += MAC_SIZE;

		memcpy(buffer.Pointer(offset), entry.Nonce, NONCE_SIZE);
		offset += NONCE_SIZE;

		memcpy(
			buffer.Pointer(offset),
			entry.EncryptedKey,
			KEY_SIZE);
		offset += KEY_SIZE;
	}

	*buffer.SwitchType<int64_t>(offset) = header.Timestamp;
	offset += sizeof(header.Timestamp);

	*buffer.SwitchType<int32_t>(offset) = header.Index;
	offset += sizeof(header.Index);

	memcpy(
		buffer.Pointer(offset),
		header.ThreadID.GetValue(),
		(uint64_t)ObjectStorage::Constants::IDSize);
	offset += (uint64_t)ObjectStorage::Constants::IDSize;

	*buffer.SwitchType<uint64_t>(offset) = header.MessageSize;
	offset += sizeof(header.MessageSize);

	memcpy(buffer.Pointer(offset), header.Nonce, NONCE_SIZE);
	offset += NONCE_SIZE;

	return buffer;
}

void Message::X25519::WriteHeaderSize(HeaderGroup &header)
{
	header.HeaderSize =
		1 +
		sizeof(int32_t) +
		BuiltStringSize(header.Source) +
		KEY_SIZE +
		BuiltStringSize(header.GroupName) +
		sizeof(int32_t) +
		header.GroupKeys.Size() * (uint64_t)GroupKeyEntrySize +
		sizeof(header.Timestamp) +
		sizeof(header.Index) +
		(uint64_t)ObjectStorage::Constants::IDSize +
		sizeof(header.MessageSize) +
		NONCE_SIZE;
}
