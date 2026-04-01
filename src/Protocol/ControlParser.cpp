#include "ControlParser.hpp"

#include "../Crypto/CryptoDefinitions.hpp"

bool CommandAddUser::ParseRequest(
	const CowBuffer<uint8_t> buffer,
	Request &request)
{
	int32_t command;
	int32_t nameLength;
	uint64_t baseLength = sizeof(command) + KEY_SIZE + sizeof(nameLength);

	if (buffer.Size() < baseLength) {
		return false;
	}

	command = *buffer.SwitchType<int32_t>();

	if (command != COMMAND_ADD_USER) {
		return false;
	}

	request.Key = buffer.Pointer(sizeof(command));

	nameLength = *buffer.SwitchType<int32_t>(sizeof(command) + KEY_SIZE);

	if (nameLength > 200 || nameLength <= 0) {
		return false;
	}

	if (buffer.Size() != baseLength + nameLength) {
		return false;
	}

	request.Name = String(
		buffer.SwitchType<char>(baseLength),
		nameLength);

	return true;
}

CowBuffer<uint8_t> CommandAddUser::BuildRequest(const Request &request)
{
	int32_t command = COMMAND_ADD_USER;
	int32_t nameLength = request.Name.Length();

	CowBuffer<uint8_t> buffer(
		sizeof(command) + KEY_SIZE + sizeof(nameLength) +
		nameLength);

	*buffer.SwitchType<int32_t>() = command;
	memcpy(buffer.Pointer(sizeof(command)), request.Key, KEY_SIZE);
	*buffer.SwitchType<int32_t>(sizeof(command) + KEY_SIZE) = nameLength;
	memcpy(
		buffer.Pointer(sizeof(command) + KEY_SIZE + sizeof(nameLength)),
		request.Name.CStr(),
		request.Name.Length());

	return buffer;
}

bool CommandAddUser::ParseResponse(
	const CowBuffer<uint8_t> buffer,
	Response &response)
{
	if (buffer.Size() != sizeof(response.Code)) {
		return false;
	}

	response.Code = *buffer.SwitchType<int32_t>();
	return true;
}

CowBuffer<uint8_t> CommandAddUser::BuildResponse(const Response &response)
{
	CowBuffer<uint8_t> buffer(sizeof(response.Code));
	*buffer.SwitchType<int32_t>() = response.Code;
	return buffer;
}

bool CommandRemoveUser::ParseRequest(
	const CowBuffer<uint8_t> buffer,
	Request &request)
{
	int32_t command;
	int32_t nameLength;

	uint64_t baseSize = sizeof(command) + sizeof(nameLength);

	if (buffer.Size() < baseSize) {
		return false;
	}

	command = *buffer.SwitchType<int32_t>();

	if (command != COMMAND_REMOVE_USER) {
		return false;
	}

	nameLength = *buffer.SwitchType<int32_t>(sizeof(command));

	if (nameLength > 200 || nameLength <= 0) {
		return false;
	}

	if (buffer.Size() != baseSize + nameLength)
	{
		return false;
	}

	String name(buffer.SwitchType<char>(baseSize), nameLength);
	request.Name = name;
	return true;
}

CowBuffer<uint8_t> CommandRemoveUser::BuildRequest(const Request &request)
{
	int32_t command = COMMAND_REMOVE_USER;
	int32_t nameLength = request.Name.Length();

	CowBuffer<uint8_t> buffer(sizeof(command) + sizeof(nameLength) +
		request.Name.Length());

	*buffer.SwitchType<int32_t>() = command;
	*buffer.SwitchType<int32_t>(sizeof(command)) = nameLength;
	memcpy(
		buffer.Pointer(sizeof(command) + sizeof(nameLength)),
		request.Name.CStr(),
		request.Name.Length());
	return buffer;
}

bool CommandRemoveUser::ParseResponse(
	const CowBuffer<uint8_t> buffer,
	Response &response)
{
	if (buffer.Size() != sizeof(response.Code)) {
		return false;
	}

	response.Code = *buffer.SwitchType<int32_t>();
	return true;
}

CowBuffer<uint8_t> CommandRemoveUser::BuildResponse(const Response &response)
{
	CowBuffer<uint8_t> buffer(sizeof(response.Code));
	*buffer.SwitchType<int32_t>() = response.Code;
	return buffer;
}

bool CommandListUsers::ParseRequest(
	const CowBuffer<uint8_t> buffer,
	Request &request)
{
	int32_t command;
	int32_t flags;

	if (buffer.Size() != sizeof(command) + sizeof(flags)) {
		return false;
	}

	command = *buffer.SwitchType<int32_t>();

	if (command != COMMAND_LIST_USERS) {
		return false;
	}

	request.Flags = *buffer.SwitchType<int32_t>(sizeof(command));
	return true;
}

CowBuffer<uint8_t> CommandListUsers::BuildRequest(const Request &request)
{
	CowBuffer<uint8_t> buffer(sizeof(int32_t) * 2);
	*buffer.SwitchType<int32_t>() = COMMAND_LIST_USERS;
	*buffer.SwitchType<int32_t>(sizeof(int32_t)) = request.Flags;
	return buffer;
}

bool CommandListUsers::ParseResponse(
	const CowBuffer<uint8_t> buffer,
	Response &response)
{
	int32_t code;

	if (buffer.Size() < sizeof(code)) {
		return false;
	}

	code = *buffer.SwitchType<int32_t>();

	if (code != OK) {
		response.Code = code;
		response.Flags = 0;
		response.Data = CowBuffer<Response::UserData>();
		return true;
	}

	int32_t userCount;
	int32_t flags;

	uint64_t baseSize = sizeof(code) + sizeof(userCount) + sizeof(flags);

	if (buffer.Size() < baseSize) {
		return false;
	}

	userCount = *buffer.SwitchType<int32_t>(sizeof(code));
	flags = *buffer.SwitchType<int32_t>(sizeof(code) + sizeof(userCount));

	if (userCount < 0) {
		return false;
	}

	CowBuffer<Response::UserData> userData(userCount);
	uint64_t offset = baseSize;

	for (int32_t i = 0; i < userCount; i++) {
		const CowBuffer<uint8_t> sliceName = buffer.Slice(
			offset,
			buffer.Size() - offset);

		int32_t nameLength;

		if (sliceName.Size() < sizeof(nameLength)) {
			return false;
		}

		nameLength = *sliceName.SwitchType<int32_t>();

		if (nameLength > 200 || nameLength <= 0) {
			return false;
		}

		if (sliceName.Size() < sizeof(nameLength) + nameLength) {
			return false;
		}

		userData[i].Name = String(
			sliceName.SwitchType<char>(sizeof(nameLength)),
			nameLength);

		offset += sizeof(nameLength) + nameLength;

		const CowBuffer<uint8_t> sliceKey = buffer.Slice(
			offset,
			buffer.Size() - offset);

		if (flags & ShowKeys) {
			if (sliceKey.Size() < KEY_SIZE) {
				return false;
			}

			userData[i].Key = sliceKey.Pointer();
			offset += KEY_SIZE;
		} else {
			userData[i].Key = nullptr;
		}
	}

	response.Code = code;
	response.Flags = flags;
	response.Data = userData;

	return true;
}

CowBuffer<uint8_t> CommandListUsers::BuildResponse(const Response &response)
{
	int32_t userCount = response.Data.Size();

	int32_t bufferSize = sizeof(response.Code) + sizeof(response.Flags) +
		sizeof(userCount);

	for (int32_t i = 0 ; i < userCount; i++) {
		bufferSize +=
			sizeof(int32_t) + response.Data[i].Name.Length();

		if (response.Flags & ShowKeys) {
			bufferSize += KEY_SIZE;
		}
	}

	CowBuffer<uint8_t> buffer(bufferSize);

	*buffer.SwitchType<int32_t>() = response.Code;
	*buffer.SwitchType<int32_t>(sizeof(int32_t)) = userCount;
	*buffer.SwitchType<int32_t>(sizeof(int32_t) * 2) = response.Flags;

	uint64_t offset = sizeof(int32_t) * 3;

	for (int32_t i = 0; i < userCount; i++) {
		*buffer.SwitchType<int32_t>(offset) =
			response.Data[i].Name.Length();
		offset += sizeof(int32_t);

		memcpy(
			buffer.Pointer(offset),
			response.Data[i].Name.CStr(),
			response.Data[i].Name.Length());

		offset += response.Data[i].Name.Length();

		if (response.Flags & ShowKeys) {
			memcpy(
				buffer.Pointer(offset),
				response.Data[i].Key,
				KEY_SIZE);
			offset += KEY_SIZE;
		}
	}

	return buffer;
}
