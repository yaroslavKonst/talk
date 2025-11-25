#include "ActiveSession.hpp"

#include <cstring>

#include "../Message/Message.hpp"

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

int32_t CommandTextMessage::ParseCommand(
	const CowBuffer<uint8_t> buffer,
	Command &result)
{
	if (buffer.Size() <= sizeof(int32_t) + Message::HeaderSize) {
		return SESSION_RESPONSE_ERROR_MESSAGE_TOO_SHORT;
	}

	int32_t command = *buffer.SwitchType<int32_t>();

	if (command != SESSION_COMMAND_TEXT_MESSAGE) {
		return SESSION_RESPONSE_ERROR;
	}

	result.Message = buffer.Slice(
		sizeof(command),
		buffer.Size() - sizeof(command));

	return SESSION_RESPONSE_OK;
}

CowBuffer<uint8_t> CommandTextMessage::BuildCommand(const Command &data)
{
	CowBuffer<uint8_t> commandBuffer(sizeof(int32_t));
	*commandBuffer.SwitchType<int32_t>() = SESSION_COMMAND_TEXT_MESSAGE;
	return commandBuffer.Concat(data.Message);
}

bool CommandTextMessage::ParseResponse(
	const CowBuffer<uint8_t> buffer,
	Response &result)
{
	if (buffer.Size() != sizeof(int32_t) + sizeof(result.Status)) {
		return false;
	}

	int32_t command = *buffer.SwitchType<int32_t>();

	if (command != SESSION_COMMAND_TEXT_MESSAGE) {
		return false;
	}

	result.Status = *buffer.SwitchType<int32_t>(sizeof(command));
	return true;
}

CowBuffer<uint8_t> CommandTextMessage::BuildResponse(const Response &data)
{
	CowBuffer<uint8_t> result(sizeof(int32_t) + sizeof(data.Status));
	*result.SwitchType<int32_t>() = SESSION_COMMAND_TEXT_MESSAGE;
	*result.SwitchType<int32_t>(sizeof(int32_t)) = data.Status;
	return result;
}

bool CommandDeliverMessage::ParseCommand(
	const CowBuffer<uint8_t> buffer,
	Command &result)
{
	if (buffer.Size() <= sizeof(int32_t) + Message::HeaderSize) {
		return false;
	}

	int32_t command = *buffer.SwitchType<int32_t>();

	if (command != SESSION_COMMAND_DELIVER_MESSAGE) {
		return false;
	}

	result.Message = buffer.Slice(
		sizeof(command),
		buffer.Size() - sizeof(command));

	return true;
}

CowBuffer<uint8_t> CommandDeliverMessage::BuildCommand(const Command &data)
{
	CowBuffer<uint8_t> commandBuffer(sizeof(int32_t));
	*commandBuffer.SwitchType<int32_t>() = SESSION_COMMAND_DELIVER_MESSAGE;
	return commandBuffer.Concat(data.Message);
}

CowBuffer<uint8_t> CommandListUsers::BuildCommand()
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
}
