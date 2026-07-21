#include "SessionParser.hpp"

#include <cstring>

#include "ParserHelpers.hpp"
//#include "../Message/Message.hpp"

bool CommandKeepAlive::ParseCommand(
	const CowBuffer<uint8_t> buffer,
	Command &result)
{
	if (buffer.Size() != sizeof(int32_t) + sizeof(int64_t)) {
		return false;
	}

	int32_t command = *buffer.SwitchType<int32_t>();

	if (command != SESSION_COMMAND_KEEP_ALIVE) {
		return false;
	}

	result.Timestamp = *buffer.SwitchType<int64_t>(sizeof(int32_t));
	return true;
}

CowBuffer<uint8_t> CommandKeepAlive::BuildCommand(const Command &data)
{
	CowBuffer<uint8_t> result(sizeof(int32_t) + sizeof(data.Timestamp));
	*result.SwitchType<int32_t>() = SESSION_COMMAND_KEEP_ALIVE;
	*result.SwitchType<int64_t>((sizeof(int32_t))) = data.Timestamp;
	return result;
}

CowBuffer<uint8_t> CommandGetHostName::BuildCommand()
{
	CowBuffer<uint8_t> buffer(sizeof(int32_t));
	*buffer.SwitchType<int32_t>() = SESSION_COMMAND_GET_HOST_NAME;
	return buffer;
}

bool CommandGetHostName::ParseResponse(
	const CowBuffer<uint8_t> buffer,
	Response &result)
{
	if (buffer.Size() < sizeof(int32_t)) {
		return false;
	}

	int32_t command = *buffer.SwitchType<int32_t>();

	if (command != SESSION_COMMAND_GET_HOST_NAME) {
		return false;
	}

	uint64_t offset = sizeof(int32_t);

	if (!ParseString(buffer, offset, result.Name, 500)) {
		return false;
	}

	if (result.Name.Length() == 0 || offset != buffer.Size()) {
		return false;
	}

	return true;
}

CowBuffer<uint8_t> CommandGetHostName::BuildResponse(const Response &data)
{
	CowBuffer<uint8_t> buffer(
		sizeof(int32_t) + BuiltStringSize(data.Name));

	*buffer.SwitchType<int32_t>() = SESSION_COMMAND_GET_HOST_NAME;

	uint64_t offset = sizeof(int32_t);

	BuildString(buffer, offset, data.Name);

	return buffer;
}

CowBuffer<uint8_t> CommandRequestID::BuildCommand()
{
	CowBuffer<uint8_t> buffer(sizeof(int32_t));
	*buffer.SwitchType<int32_t>() = SESSION_COMMAND_REQUEST_ID;
	return buffer;
}

bool CommandRequestID::ParseResponse(
	const CowBuffer<uint8_t> buffer, Response &result)
{
	uint32_t validSize =
		sizeof(int32_t) + (int)ObjectStorage::Constants::IDSize;

	if (buffer.Size() != validSize) {
		return false;
	}

	if (*buffer.SwitchType<int32_t>() != SESSION_COMMAND_REQUEST_ID) {
		return false;
	}

	result.Id = ObjectStorage::ID(buffer.Pointer(sizeof(int32_t)));
	return true;
}

CowBuffer<uint8_t> CommandRequestID::BuildResponse(const Response &data)
{
	CowBuffer<uint8_t> buffer(
		sizeof(int32_t) + (int)ObjectStorage::Constants::IDSize);

	*buffer.SwitchType<int32_t>() = SESSION_COMMAND_REQUEST_ID;

	memcpy(
		buffer.Pointer(sizeof(int32_t)),
		data.Id.GetValue(),
		(int)ObjectStorage::Constants::IDSize);

	return buffer;
}

bool CommandUpdateID::ParseCommand(
	const CowBuffer<uint8_t> buffer,
	Command &result)
{
	uint32_t validSize =
		sizeof(int32_t) + (int)ObjectStorage::Constants::IDSize;

	if (buffer.Size() != validSize) {
		return false;
	}

	if (*buffer.SwitchType<int32_t>() != SESSION_COMMAND_UPDATE_ID) {
		return false;
	}

	result.Id = ObjectStorage::ID(buffer.Pointer(sizeof(int32_t)));
	return true;
}

CowBuffer<uint8_t> CommandUpdateID::BuildCommand(const Command &command)
{
	CowBuffer<uint8_t> buffer(
		sizeof(int32_t) + (int)ObjectStorage::Constants::IDSize);

	*buffer.SwitchType<int32_t>() = SESSION_COMMAND_UPDATE_ID;

	memcpy(
		buffer.Pointer(sizeof(int32_t)),
		command.Id.GetValue(),
		(int)ObjectStorage::Constants::IDSize);

	return buffer;
}

bool CommandAddContact::ParseCommand(
	const CowBuffer<uint8_t> buffer,
	Command &result)
{
	if (buffer.Size() < sizeof(int32_t)) {
		return false;
	}

	int32_t command = *buffer.SwitchType<int32_t>();

	if (command != SESSION_COMMAND_ADD_CONTACT) {
		return false;
	}

	uint64_t offset = sizeof(int32_t);

	if (!ParseString(buffer, offset, result.ContactName, 500)) {
		return false;
	}

	if (result.ContactName.Length() == 0 || offset != buffer.Size()) {
		return false;
	}

	return true;
}

CowBuffer<uint8_t> CommandAddContact::BuildCommand(const Command &command)
{
	CowBuffer<uint8_t> buffer(
		sizeof(int32_t) + BuiltStringSize(command.ContactName));

	*buffer.SwitchType<int32_t>() = SESSION_COMMAND_ADD_CONTACT;

	uint64_t offset = sizeof(int32_t);

	BuildString(buffer, offset, command.ContactName);

	return buffer;
}

bool CommandUpdateContactKey::ParseCommand(
	const CowBuffer<uint8_t> buffer,
	Command &result)
{
	if (buffer.Size() < sizeof(int32_t)) {
		return false;
	}

	int32_t command = *buffer.SwitchType<int32_t>();

	if (command != SESSION_COMMAND_UPDATE_CONTACT_KEY) {
		return false;
	}

	uint64_t offset = sizeof(int32_t);

	if (!ParseString(buffer, offset, result.ContactName, 500)) {
		return false;
	}

	if (result.ContactName.Length() == 0) {
		return false;
	}

	uint64_t tailSize = Crypto::X25519::KEY_SIZE + 3;

	if (buffer.Size() != offset + tailSize) {
		return false;
	}

	result.Key = buffer.Pointer(offset);
	offset += Crypto::X25519::KEY_SIZE;

	result.Validated = *buffer.Pointer(offset);
	offset += 1;

	result.Blocked = *buffer.Pointer(offset);
	offset += 1;

	result.SetAsDefault = *buffer.Pointer(offset);

	return true;
}

CowBuffer<uint8_t> CommandUpdateContactKey::BuildCommand(const Command &command)
{
	uint64_t tailSize = Crypto::X25519::KEY_SIZE + 3;

	CowBuffer<uint8_t> buffer(
		sizeof(int32_t) +
		BuiltStringSize(command.ContactName) +
		tailSize);

	*buffer.SwitchType<int32_t>() = SESSION_COMMAND_UPDATE_CONTACT_KEY;

	uint64_t offset = sizeof(int32_t);

	BuildString(buffer, offset, command.ContactName);

	memcpy(
		buffer.Pointer(offset),
		command.Key.Key,
		Crypto::X25519::KEY_SIZE);
	offset += Crypto::X25519::KEY_SIZE;

	*buffer.Pointer(offset) = command.Validated;
	offset += 1;

	*buffer.Pointer(offset) = command.Blocked;
	offset += 1;

	*buffer.Pointer(offset) = command.SetAsDefault;

	return buffer;
}

bool CommandBlockContact::ParseCommand(
	const CowBuffer<uint8_t> buffer,
	Command &result)
{
	if (buffer.Size() < sizeof(int32_t)) {
		return false;
	}

	int32_t command = *buffer.SwitchType<int32_t>();

	if (command != SESSION_COMMAND_BLOCK_CONTACT) {
		return false;
	}

	uint64_t offset = sizeof(int32_t);

	if (!ParseString(buffer, offset, result.ContactName, 500)) {
		return false;
	}

	if (result.ContactName.Length() == 0) {
		return false;
	}

	if (buffer.Size() != offset + sizeof(uint8_t)) {
		return false;
	}

	result.BlockStatus = *buffer.Pointer(offset);

	return true;
}

CowBuffer<uint8_t> CommandBlockContact::BuildCommand(const Command &command)
{
	CowBuffer<uint8_t> buffer(
		sizeof(int32_t) +
		BuiltStringSize(command.ContactName) +
		sizeof(uint8_t));

	*buffer.SwitchType<int32_t>() = SESSION_COMMAND_BLOCK_CONTACT;

	uint64_t offset = sizeof(int32_t);

	BuildString(buffer, offset, command.ContactName);

	*buffer.Pointer(offset) = command.BlockStatus;

	return buffer;
}

bool CommandSendMessage::ParseCommand(
	const CowBuffer<uint8_t> buffer,
	Command &result)
{
	if (buffer.Size() <= sizeof(int32_t) * 2) {
		return false;
	}

	int32_t command = *buffer.SwitchType<int32_t>();

	if (command != SESSION_COMMAND_SEND_MESSAGE) {
		return false;
	}

	result.Message = buffer.Slice(
		sizeof(command),
		buffer.Size() - sizeof(command));

	return true;
}

CowBuffer<uint8_t> CommandSendMessage::BuildCommand(const Command &data)
{
	CowBuffer<uint8_t> commandBuffer(sizeof(int32_t));
	*commandBuffer.SwitchType<int32_t>() = SESSION_COMMAND_SEND_MESSAGE;
	return commandBuffer.Concat(data.Message);
}

bool CommandUpdateMessage::ParseCommand(
	const CowBuffer<uint8_t> buffer,
	Command &result)
{
	if (buffer.Size() < sizeof(int32_t)) {
		return false;
	}

	int32_t command = *buffer.SwitchType<int32_t>();

	if (command != SESSION_COMMAND_UPDATE_MESSAGE) {
		return false;
	}

	uint64_t offset = sizeof(int32_t);

	bool status = ParseString(
		buffer,
		offset,
		result.PeerName);

	if (!status) {
		return false;
	}

	if (buffer.Size() < offset + (int)ObjectStorage::Constants::IDSize) {
		return false;
	}

	result.HeaderHash = buffer.Slice(
		offset,
		(int)ObjectStorage::Constants::IDSize);
	offset += (int)ObjectStorage::Constants::IDSize;

	if (buffer.Size() < offset + sizeof(Message::Attribute)) {
		return false;
	}

	result.Attr = *buffer.SwitchType<Message::Attribute>(offset);
	offset += sizeof(Message::Attribute);

	if (buffer.Size() != offset + sizeof(uint8_t)) {
		return false;
	}

	result.AttrValue = *buffer.SwitchType<uint8_t>(offset);

	return true;
}

CowBuffer<uint8_t> CommandUpdateMessage::BuildCommand(const Command &data)
{
	CowBuffer<uint8_t> buffer(
		sizeof(int32_t) +
		BuiltStringSize(data.PeerName) +
		(int)ObjectStorage::Constants::IDSize +
		sizeof(data.Attr) + sizeof(data.AttrValue));

	*buffer.SwitchType<int32_t>() = SESSION_COMMAND_UPDATE_MESSAGE;

	uint64_t offset = sizeof(int32_t);

	BuildString(buffer, offset, data.PeerName);

	memcpy(
		buffer.Pointer(offset),
		data.HeaderHash.Pointer(),
		(int)ObjectStorage::Constants::IDSize);
	offset += (int)ObjectStorage::Constants::IDSize;

	*buffer.SwitchType<Message::Attribute>(offset) = data.Attr;
	offset += sizeof(data.Attr);

	*buffer.Pointer(offset) = data.AttrValue;

	return buffer;
}

/*CowBuffer<uint8_t> CommandListUsers::BuildCommand()
{
	CowBuffer<uint8_t> commandBuffer(sizeof(int32_t));
	*commandBuffer.SwitchType<int32_t>() = SESSION_COMMAND_LIST_USERS;
	return commandBuffer;
}

bool CommandListUsers::ParseResponse(
	const CowBuffer<uint8_t> buffer,
	Response &result)
{
	int nameLength = 55;

	if (buffer.Size() < sizeof(int32_t) * 2) {
		return false;
	}

	int32_t command = *buffer.SwitchType<int32_t>();

	if (command != SESSION_COMMAND_LIST_USERS) {
		return false;
	}

	int32_t userCount = *buffer.SwitchType<int32_t>(sizeof(command));

	if (buffer.Size() !=
		sizeof(int32_t) * 2 + (KEY_SIZE + nameLength) * userCount)
	{
		return false;
	}

	result.Data.Resize(userCount);

	for (int i = 0; i < userCount; i++) {
		result.Data[i].Key = buffer.Pointer(
			sizeof(int32_t) * 2 + i * (KEY_SIZE + nameLength));
		result.Data[i].Name = buffer.SwitchType<char>(
			sizeof(int32_t) * 2 + i * (KEY_SIZE + nameLength) +
			KEY_SIZE);
	}

	return true;
}

CowBuffer<uint8_t> CommandListUsers::BuildResponse(const Response &data)
{
	int nameLength = 55;

	CowBuffer<uint8_t> result(sizeof(int32_t) * 2 +
		(KEY_SIZE + nameLength) * data.Data.Size());

	*result.SwitchType<int32_t>() = SESSION_COMMAND_LIST_USERS;
	*result.SwitchType<int32_t>(sizeof(int32_t)) = data.Data.Size();

	memset(
		result.Pointer(sizeof(int32_t) * 2),
		0,
		result.Size() - sizeof(int32_t) * 2);

	for (unsigned int i = 0; i < data.Data.Size(); i++) {
		memcpy(
			result.Pointer(sizeof(int32_t) * 2 +
				i * (KEY_SIZE + nameLength)),
			data.Data[i].Key,
			KEY_SIZE);

		memcpy(
			result.Pointer(sizeof(int32_t) * 2 +
				i * (KEY_SIZE + nameLength) + KEY_SIZE),
			data.Data[i].Name.CStr(),
			data.Data[i].Name.Length() + 1);
	}

	return result;
}

bool CommandGetMessages::ParseCommand(
	const CowBuffer<uint8_t> buffer,
	Command &result)
{
	if (buffer.Size() != sizeof(int32_t) + sizeof(result.Timestamp)) {
		return false;
	}

	int32_t command = *buffer.SwitchType<int32_t>();

	if (command != SESSION_COMMAND_GET_MESSAGES) {
		return false;
	}

	result.Timestamp = *buffer.SwitchType<int64_t>(sizeof(command));
	return true;
}

CowBuffer<uint8_t> CommandGetMessages::BuildCommand(const Command &data)
{
	CowBuffer<uint8_t> result(sizeof(int32_t) + sizeof(data.Timestamp));
	*result.SwitchType<int32_t>() = SESSION_COMMAND_GET_MESSAGES;
	*result.SwitchType<int64_t>(sizeof(int32_t)) = data.Timestamp;
	return result;
}

bool CommandVoiceInit::ParseCommand(
	const CowBuffer<uint8_t> buffer,
	Command &result)
{
	if (buffer.Size() != sizeof(int32_t) + KEY_SIZE + sizeof(int64_t)) {
		return false;
	}

	int32_t command = *buffer.SwitchType<int32_t>();

	if (command != SESSION_COMMAND_VOICE_INIT) {
		return false;
	}

	result.Key = buffer.Pointer(sizeof(command));
	result.Timestamp = *buffer.SwitchType<int64_t>(
		sizeof(command) + KEY_SIZE);

	return true;
}

CowBuffer<uint8_t> CommandVoiceInit::BuildCommand(const Command &data)
{
	CowBuffer<uint8_t> result(
		sizeof(int32_t) + KEY_SIZE + sizeof(data.Timestamp));

	*result.SwitchType<int32_t>() = SESSION_COMMAND_VOICE_INIT;
	memcpy(result.Pointer(sizeof(int32_t)), data.Key, KEY_SIZE);
	*result.SwitchType<int64_t>(sizeof(int32_t) + KEY_SIZE) =
		data.Timestamp;

	return result;
}

bool CommandVoiceInit::ParseResponse(
	const CowBuffer<uint8_t> buffer,
	Response &result)
{
	if (buffer.Size() != sizeof(int32_t) + sizeof(result.Status)) {
		return false;
	}

	int32_t command = *buffer.SwitchType<int32_t>();

	if (command != SESSION_COMMAND_VOICE_INIT) {
		return false;
	}

	result.Status = *buffer.SwitchType<int32_t>(sizeof(command));
	return true;
}

CowBuffer<uint8_t> CommandVoiceInit::BuildResponse(const Response &data)
{
	CowBuffer<uint8_t> result(sizeof(int32_t) + sizeof(data.Status));
	*result.SwitchType<int32_t>() = SESSION_COMMAND_VOICE_INIT;
	*result.SwitchType<int32_t>(sizeof(int32_t)) = data.Status;
	return result;
}

bool CommandVoiceRequest::ParseCommand(
	const CowBuffer<uint8_t> buffer,
	Command &result)
{
	if (buffer.Size() != sizeof(int32_t) + KEY_SIZE + sizeof(int64_t)) {
		return false;
	}

	int32_t command = *buffer.SwitchType<int32_t>();

	if (command != SESSION_COMMAND_VOICE_REQUEST) {
		return false;
	}

	result.Key = buffer.Pointer(sizeof(command));
	result.Timestamp = *buffer.SwitchType<int64_t>(
		sizeof(command) + KEY_SIZE);

	return true;
}

CowBuffer<uint8_t> CommandVoiceRequest::BuildCommand(const Command &data)
{
	CowBuffer<uint8_t> result(
		sizeof(int32_t) + KEY_SIZE + sizeof(data.Timestamp));

	*result.SwitchType<int32_t>() = SESSION_COMMAND_VOICE_REQUEST;
	memcpy(result.Pointer(sizeof(int32_t)), data.Key, KEY_SIZE);
	*result.SwitchType<int64_t>(sizeof(int32_t) + KEY_SIZE) =
		data.Timestamp;

	return result;
}

bool CommandVoiceRequest::ParseResponse(
	const CowBuffer<uint8_t> buffer,
	Response &result)
{
	if (buffer.Size() != sizeof(int32_t) + sizeof(result.Status)) {
		return false;
	}

	int32_t command = *buffer.SwitchType<int32_t>();

	if (command != SESSION_COMMAND_VOICE_REQUEST) {
		return false;
	}

	result.Status = *buffer.SwitchType<int32_t>(sizeof(command));
	return true;
}

CowBuffer<uint8_t> CommandVoiceRequest::BuildResponse(const Response &data)
{
	CowBuffer<uint8_t> result(sizeof(int32_t) + sizeof(data.Status));
	*result.SwitchType<int32_t>() = SESSION_COMMAND_VOICE_REQUEST;
	*result.SwitchType<int32_t>(sizeof(int32_t)) = data.Status;
	return result;
}

CowBuffer<uint8_t> CommandVoiceEnd::BuildCommand()
{
	CowBuffer<uint8_t> result(sizeof(int32_t));
	*result.SwitchType<int32_t>() = SESSION_COMMAND_VOICE_END;
	return result;
}

bool CommandVoiceData::ParseCommand(
	const CowBuffer<uint8_t> buffer,
	Command &result)
{
	if (buffer.Size() <= sizeof(int32_t)) {
		return false;
	}

	int32_t command = *buffer.SwitchType<int32_t>();

	if (command != SESSION_COMMAND_VOICE_DATA) {
		return false;
	}

	result.VoiceData = buffer.Slice(
		sizeof(command),
		buffer.Size() - sizeof(command));

	return true;
}

CowBuffer<uint8_t> CommandVoiceData::BuildCommand(const Command &data)
{
	CowBuffer<uint8_t> commandBuffer(sizeof(int32_t));
	*commandBuffer.SwitchType<int32_t>() = SESSION_COMMAND_VOICE_DATA;
	return commandBuffer.Concat(data.VoiceData);
}*/
