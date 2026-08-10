#include "ObjectType.hpp"

#include <cstring>

#include "../Common/ObjectStorage.hpp"
#include "../Protocol/ParserHelpers.hpp"

bool NewContactObject::ParseData(const CowBuffer<uint8_t> object, Data &data)
{
	uint64_t headerSize =
		sizeof(int32_t) + (int)ObjectStorage::Constants::IDSize;

	if (object.Size() < headerSize) {
		return false;
	}

	int32_t type = *object.SwitchType<int32_t>();

	if (type != (int32_t)ObjectType::NewContact) {
		return false;
	}

	uint64_t offset = headerSize;

	if (!ParseString(object, offset, data.ContactName, 500)) {
		return false;
	}

	if (data.ContactName.Length() == 0) {
		return false;
	}

	if (object.Size() != offset) {
		return false;
	}

	return true;
}

CowBuffer<uint8_t> NewContactObject::BuildData(const Data &data)
{
	uint64_t idSize = (int)ObjectStorage::Constants::IDSize;

	CowBuffer<uint8_t> buffer(
		sizeof(int32_t) +
		idSize +
		BuiltStringSize(data.ContactName));

	*buffer.SwitchType<int32_t>() = (int32_t)ObjectType::NewContact;

	uint64_t offset = sizeof(int32_t);

	memset(buffer.Pointer(offset), 0, idSize);
	offset += idSize;

	BuildString(buffer, offset, data.ContactName);

	return buffer;
}

bool UpdateContactKeyObject::ParseData(
	const CowBuffer<uint8_t> object,
	Data &data)
{
	uint64_t headerSize =
		sizeof(int32_t) + (int)ObjectStorage::Constants::IDSize;

	if (object.Size() < headerSize) {
		return false;
	}

	int32_t type = *object.SwitchType<int32_t>();

	if (type != (int32_t)ObjectType::UpdateContactKey) {
		return false;
	}

	uint64_t offset = headerSize;

	if (!ParseString(object, offset, data.ContactName, 500)) {
		return false;
	}

	if (data.ContactName.Length() == 0) {
		return false;
	}

	uint64_t tailSize = Crypto::X25519::KEY_SIZE + 3;

	if (object.Size() != offset + tailSize) {
		return false;
	}

	data.Key = object.Pointer(offset);
	offset += Crypto::X25519::KEY_SIZE;

	data.Validated = *object.Pointer(offset);
	offset += 1;

	data.Blocked = *object.Pointer(offset);
	offset += 1;

	data.SetAsDefault = *object.Pointer(offset);

	return true;
}

CowBuffer<uint8_t> UpdateContactKeyObject::BuildData(const Data &data)
{
	uint64_t idSize = (int)ObjectStorage::Constants::IDSize;
	uint64_t tailSize = Crypto::X25519::KEY_SIZE + 3;

	CowBuffer<uint8_t> buffer(
		sizeof(int32_t) +
		idSize +
		BuiltStringSize(data.ContactName) +
		tailSize);

	*buffer.SwitchType<int32_t>() = (int32_t)ObjectType::UpdateContactKey;

	uint64_t offset = sizeof(int32_t);

	memset(buffer.Pointer(offset), 0, idSize);
	offset += idSize;

	BuildString(buffer, offset, data.ContactName);

	memcpy(
		buffer.Pointer(offset),
		data.Key.Key,
		Crypto::X25519::KEY_SIZE);
	offset += Crypto::X25519::KEY_SIZE;

	*buffer.Pointer(offset) = data.Validated;
	offset += 1;

	*buffer.Pointer(offset) = data.Blocked;
	offset += 1;

	*buffer.Pointer(offset) = data.SetAsDefault;

	return buffer;
}

bool BlockContactObject::ParseData(const CowBuffer<uint8_t> object, Data &data)
{
	uint64_t headerSize =
		sizeof(int32_t) + (int)ObjectStorage::Constants::IDSize;

	if (object.Size() < headerSize) {
		return false;
	}

	int32_t type = *object.SwitchType<int32_t>();

	if (type != (int32_t)ObjectType::BlockContact) {
		return false;
	}

	uint64_t offset = headerSize;

	if (!ParseString(object, offset, data.ContactName, 500)) {
		return false;
	}

	if (data.ContactName.Length() == 0) {
		return false;
	}

	if (object.Size() != offset + sizeof(uint8_t)) {
		return false;
	}

	data.BlockStatus = *object.Pointer(offset);

	return true;
}

CowBuffer<uint8_t> BlockContactObject::BuildData(const Data &data)
{
	uint64_t idSize = (int)ObjectStorage::Constants::IDSize;

	CowBuffer<uint8_t> buffer(
		sizeof(int32_t) +
		idSize +
		BuiltStringSize(data.ContactName) +
		sizeof(uint8_t));

	*buffer.SwitchType<int32_t>() = (int32_t)ObjectType::BlockContact;

	uint64_t offset = sizeof(int32_t);

	memset(buffer.Pointer(offset), 0, idSize);
	offset += idSize;

	BuildString(buffer, offset, data.ContactName);

	*buffer.Pointer(offset) = data.BlockStatus;

	return buffer;
}

bool RemoveContactObject::ParseData(const CowBuffer<uint8_t> object, Data &data)
{
	uint64_t headerSize =
		sizeof(int32_t) + (int)ObjectStorage::Constants::IDSize;

	if (object.Size() < headerSize) {
		return false;
	}

	int32_t type = *object.SwitchType<int32_t>();

	if (type != (int32_t)ObjectType::RemoveContact) {
		return false;
	}

	uint64_t offset = headerSize;

	if (!ParseString(object, offset, data.ContactName, 500)) {
		return false;
	}

	if (data.ContactName.Length() == 0) {
		return false;
	}

	if (object.Size() != offset) {
		return false;
	}

	return true;
}

CowBuffer<uint8_t> RemoveContactObject::BuildData(const Data &data)
{
	uint64_t idSize = (int)ObjectStorage::Constants::IDSize;

	CowBuffer<uint8_t> buffer(
		sizeof(int32_t) +
		idSize +
		BuiltStringSize(data.ContactName));

	*buffer.SwitchType<int32_t>() = (int32_t)ObjectType::RemoveContact;

	uint64_t offset = sizeof(int32_t);

	memset(buffer.Pointer(offset), 0, idSize);
	offset += idSize;

	BuildString(buffer, offset, data.ContactName);

	return buffer;
}

bool MessageObject::ParseData(const CowBuffer<uint8_t> object, Data &data)
{
	uint64_t headerSize =
		sizeof(int32_t) + (int)ObjectStorage::Constants::IDSize;

	if (object.Size() < headerSize) {
		return false;
	}

	int32_t type = *object.SwitchType<int32_t>();

	if (type != (int32_t)ObjectType::Message) {
		return false;
	}

	uint64_t offset = headerSize;

	if (!ParseString(object, offset, data.PeerName, 500)) {
		return false;
	}

	if (data.PeerName.Length() == 0) {
		return false;
	}

	uint64_t tailSize = (int)ObjectStorage::Constants::IDSize;

	if (object.Size() != offset + tailSize) {
		return false;
	}

	data.HeaderHash.SetValue(object.Pointer(offset));

	return true;
}

CowBuffer<uint8_t> MessageObject::BuildData(const Data &data)
{
	uint64_t idSize = (int)ObjectStorage::Constants::IDSize;

	CowBuffer<uint8_t> buffer(
		sizeof(int32_t) +
		idSize +
		BuiltStringSize(data.PeerName) +
		idSize);

	*buffer.SwitchType<int32_t>() = (int32_t)ObjectType::Message;

	uint64_t offset = sizeof(int32_t);

	memset(buffer.Pointer(offset), 0, idSize);
	offset += idSize;

	BuildString(buffer, offset, data.PeerName);

	data.HeaderHash.GetValue(buffer.Pointer(offset));

	return buffer;
}

bool UpdateMessageObject::ParseData(const CowBuffer<uint8_t> object, Data &data)
{
	uint64_t headerSize =
		sizeof(int32_t) + (int)ObjectStorage::Constants::IDSize;

	if (object.Size() < headerSize) {
		return false;
	}

	int32_t type = *object.SwitchType<int32_t>();

	if (type != (int32_t)ObjectType::UpdateMessage) {
		return false;
	}

	uint64_t offset = headerSize;

	if (!ParseString(object, offset, data.PeerName, 500)) {
		return false;
	}

	if (data.PeerName.Length() == 0) {
		return false;
	}

	uint64_t tailSize =
		(int)ObjectStorage::Constants::IDSize +
		sizeof(int32_t) +
		sizeof(uint8_t);

	if (object.Size() != offset + tailSize) {
		return false;
	}

	data.HeaderHash.SetValue(object.Pointer(offset));
	offset += (int)ObjectStorage::Constants::IDSize;

	data.Attr = (Message::Attribute)*object.SwitchType<int32_t>(offset);
	offset += sizeof(int32_t);

	data.Value = *object.Pointer(offset);

	return true;
}

CowBuffer<uint8_t> UpdateMessageObject::BuildData(const Data &data)
{
	uint64_t idSize = (int)ObjectStorage::Constants::IDSize;
	uint64_t tailSize = idSize + sizeof(int32_t) + sizeof(uint8_t);

	CowBuffer<uint8_t> buffer(
		sizeof(int32_t) +
		idSize +
		BuiltStringSize(data.PeerName) +
		tailSize);

	*buffer.SwitchType<int32_t>() = (int32_t)ObjectType::UpdateMessage;

	uint64_t offset = sizeof(int32_t);

	memset(buffer.Pointer(offset), 0, idSize);
	offset += idSize;

	BuildString(buffer, offset, data.PeerName);

	data.HeaderHash.GetValue(buffer.Pointer(offset));
	offset += idSize;

	*buffer.SwitchType<int32_t>(offset) = (int32_t)data.Attr;
	offset += sizeof(int32_t);

	*buffer.Pointer(offset) = data.Value ? 1 : 0;

	return buffer;
}
