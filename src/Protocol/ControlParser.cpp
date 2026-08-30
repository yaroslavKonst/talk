#include "ControlParser.hpp"

#include "ParserHelpers.hpp"
#include "CommonParserConstants.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

bool CommandAddUser::ParseRequest(
	const CowBuffer<uint8_t> buffer,
	Request &request)
{
	uint64_t baseLength = sizeof(int32_t) + Crypto::X25519::KEY_SIZE;

	if (buffer.Size() < baseLength) {
		return false;
	}

	if (*buffer.SwitchType<int32_t>() != COMMAND_ADD_USER) {
		return false;
	}

	request.Key = buffer.Pointer(sizeof(int32_t));

	uint64_t offset = baseLength;

	if (!ParseString(
		buffer,
		offset,
		request.Name,
		CommonParserConstants::FullNameSize))
	{
		return false;
	}

	if (request.Name.Length() == 0 || offset != buffer.Size()) {
		return false;
	}

	return true;
}

CowBuffer<uint8_t> CommandAddUser::BuildRequest(const Request &request)
{
	CowBuffer<uint8_t> buffer(
		sizeof(int32_t) + Crypto::X25519::KEY_SIZE +
		BuiltStringSize(request.Name));

	uint64_t offset = 0;

	*buffer.SwitchType<int32_t>(offset) = COMMAND_ADD_USER;
	offset += sizeof(int32_t);

	memcpy(
		buffer.Pointer(offset),
		request.Key.Key,
		Crypto::X25519::KEY_SIZE);
	offset += Crypto::X25519::KEY_SIZE;

	BuildString(buffer, offset, request.Name);

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
	if (buffer.Size() < sizeof(int32_t)) {
		return false;
	}

	if (*buffer.SwitchType<int32_t>() != COMMAND_REMOVE_USER) {
		return false;
	}

	uint64_t offset = sizeof(int32_t);

	if (!ParseString(
		buffer,
		offset,
		request.Name,
		CommonParserConstants::FullNameSize))
	{
		return false;
	}

	if (request.Name.Length() == 0 || offset != buffer.Size()) {
		return false;
	}

	return true;
}

CowBuffer<uint8_t> CommandRemoveUser::BuildRequest(const Request &request)
{
	CowBuffer<uint8_t> buffer(
		sizeof(int32_t) + BuiltStringSize(request.Name));

	uint64_t offset = 0;

	*buffer.SwitchType<int32_t>(offset) = COMMAND_REMOVE_USER;
	offset += sizeof(int32_t);

	BuildString(buffer, offset, request.Name);

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
		if (!ParseString(
			buffer,
			offset,
			userData[i].Name,
			CommonParserConstants::FullNameSize))
		{
			return false;
		}

		if (userData[i].Name.Length() == 0) {
			return false;
		}

		if (flags & ShowKeys) {
			if (buffer.Size() < offset + Crypto::X25519::KEY_SIZE) {
				return false;
			}

			userData[i].Key = buffer.Pointer(offset);
			offset += Crypto::X25519::KEY_SIZE;
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
		bufferSize += BuiltStringSize(response.Data[i].Name);

		if (response.Flags & ShowKeys) {
			bufferSize += Crypto::X25519::KEY_SIZE;
		}
	}

	CowBuffer<uint8_t> buffer(bufferSize);

	*buffer.SwitchType<int32_t>() = response.Code;
	*buffer.SwitchType<int32_t>(sizeof(int32_t)) = userCount;
	*buffer.SwitchType<int32_t>(sizeof(int32_t) * 2) = response.Flags;

	uint64_t offset = sizeof(int32_t) * 3;

	for (int32_t i = 0; i < userCount; i++) {
		BuildString(buffer, offset, response.Data[i].Name);

		if (response.Flags & ShowKeys) {
			memcpy(
				buffer.Pointer(offset),
				response.Data[i].Key.Key,
				Crypto::X25519::KEY_SIZE);
			offset += Crypto::X25519::KEY_SIZE;
		}
	}

	return buffer;
}

bool CommandFailBanListBanned::ParseResponse(
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
		response.BannedIPList = CowBuffer<IPAddress>();
		return true;
	}

	int32_t ipCount;

	uint64_t baseSize = sizeof(code) + sizeof(ipCount);

	if (buffer.Size() < baseSize) {
		return false;
	}

	ipCount = *buffer.SwitchType<int32_t>(sizeof(code));

	if (ipCount < 0) {
		return false;
	}

	if (buffer.Size() != baseSize + ipCount * sizeof(IPAddress)) {
		return false;
	}

	CowBuffer<IPAddress> ipList(ipCount);

	for (int32_t i = 0; i < ipCount; i++) {
		ipList[i] = *buffer.SwitchType<IPAddress>(
			baseSize + i * sizeof(IPAddress));
	}

	response.Code = code;
	response.BannedIPList = ipList;

	return true;
}

CowBuffer<uint8_t> CommandFailBanListBanned::BuildResponse(
	const Response &response)
{
	int32_t ipCount = response.BannedIPList.Size();

	uint64_t bufferSize = sizeof(response.Code) + sizeof(ipCount) +
		ipCount * sizeof(IPAddress);

	CowBuffer<uint8_t> buffer(bufferSize);

	*buffer.SwitchType<int32_t>() = response.Code;
	*buffer.SwitchType<int32_t>(sizeof(response.Code)) = ipCount;

	uint64_t offset = sizeof(response.Code) + sizeof(ipCount);

	for (int32_t i = 0; i < ipCount; i++) {
		*buffer.SwitchType<IPAddress>(offset) =
			response.BannedIPList[i];
		offset += sizeof(IPAddress);
	}

	return buffer;
}

bool CommandFailBanBan::ParseRequest(
	const CowBuffer<uint8_t> buffer,
	Request &request)
{
	int32_t command;

	if (buffer.Size() != sizeof(command) + sizeof(request.IP)) {
		return false;
	}

	command = *buffer.SwitchType<int32_t>();

	if (command != COMMAND_FAILBAN_BAN) {
		return false;
	}

	request.IP = *buffer.SwitchType<IPAddress>(sizeof(command));

	return true;
}

CowBuffer<uint8_t> CommandFailBanBan::BuildRequest(const Request &request)
{
	int32_t command = COMMAND_FAILBAN_BAN;

	CowBuffer<uint8_t> buffer(sizeof(command) + sizeof(request.IP));

	*buffer.SwitchType<int32_t>() = command;
	*buffer.SwitchType<IPAddress>(sizeof(command)) = request.IP;

	return buffer;
}

bool CommandFailBanBan::ParseResponse(
	const CowBuffer<uint8_t> buffer,
	Response &response)
{
	if (buffer.Size() != sizeof(response.Code)) {
		return false;
	}

	response.Code = *buffer.SwitchType<int32_t>();
	return true;
}

CowBuffer<uint8_t> CommandFailBanBan::BuildResponse(const Response &response)
{
	CowBuffer<uint8_t> buffer(sizeof(response.Code));
	*buffer.SwitchType<int32_t>() = response.Code;
	return buffer;
}

bool CommandFailBanUnban::ParseRequest(
	const CowBuffer<uint8_t> buffer,
	Request &request)
{
	int32_t command;

	if (buffer.Size() != sizeof(command) + sizeof(request.IP)) {
		return false;
	}

	command = *buffer.SwitchType<int32_t>();

	if (command != COMMAND_FAILBAN_UNBAN) {
		return false;
	}

	request.IP = *buffer.SwitchType<IPAddress>(sizeof(command));

	return true;
}

CowBuffer<uint8_t> CommandFailBanUnban::BuildRequest(const Request &request)
{
	int32_t command = COMMAND_FAILBAN_UNBAN;

	CowBuffer<uint8_t> buffer(sizeof(command) + sizeof(request.IP));

	*buffer.SwitchType<int32_t>() = command;
	*buffer.SwitchType<IPAddress>(sizeof(command)) = request.IP;

	return buffer;
}

bool CommandFailBanUnban::ParseResponse(
	const CowBuffer<uint8_t> buffer,
	Response &response)
{
	if (buffer.Size() != sizeof(response.Code)) {
		return false;
	}

	response.Code = *buffer.SwitchType<int32_t>();
	return true;
}

CowBuffer<uint8_t> CommandFailBanUnban::BuildResponse(const Response &response)
{
	CowBuffer<uint8_t> buffer(sizeof(response.Code));
	*buffer.SwitchType<int32_t>() = response.Code;
	return buffer;
}
