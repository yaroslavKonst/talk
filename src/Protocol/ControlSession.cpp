#include "ControlSession.hpp"

#include <cstring>

#include "../ServerCtl/SocketName.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/Hex.hpp"
#include "../Common/Log.hpp"

ControlSession::ControlSession()
{
	InputSizeLimit = 1024 * 1024;
}

bool ControlSession::TimePassed()
{
	int64_t t = GetUnixTime();

	if (t - Time > 10) {
		return false;
	}

	return true;
}

bool ControlSession::Process()
{
	CowBuffer<uint8_t> message = Receive();

	if (message.Size() < sizeof(int32_t)) {
		ProcessUnknownCommand();
		return true;
	}

	int32_t command;
	memcpy(&command, message.Pointer(), sizeof(int32_t));

	switch (command) {
	case COMMAND_SHUTDOWN:
		ProcessShutdownCommand();
		break;
	case COMMAND_GET_PUBLIC_KEY:
		ProcessGetPublicKeyCommand();
		break;
	case COMMAND_ADD_USER:
		ProcessAddUserCommand(message);
		break;
	case COMMAND_REMOVE_USER:
		ProcessRemoveUserCommand(message);
		break;
	case COMMAND_LIST_USERS:
		ProcessListUsersCommand();
	default:
		ProcessUnknownCommand();
		break;
	}

	return true;
}

void ControlSession::SendResponse(int32_t code, const CowBuffer<uint8_t> data)
{
	CowBuffer<uint8_t> message(sizeof(code) + data.Size());
	memcpy(message.Pointer(), &code, sizeof(code));

	if (data.Size()) {
		memcpy(
			message.Pointer() + sizeof(code),
			data.Pointer(),
			data.Size());
	}

	Send(message, 0);
}

void ControlSession::ProcessShutdownCommand()
{
	Log("Received shutdown command.");
	*Work = false;
}

void ControlSession::ProcessGetPublicKeyCommand()
{
	CowBuffer<uint8_t> message(KEY_SIZE);
	memcpy(message.Pointer(), PublicKey, KEY_SIZE);

	SendResponse(OK, message);
}

void ControlSession::ProcessAddUserCommand(const CowBuffer<uint8_t> message)
{
	uint32_t minMessageLength =
		sizeof(int32_t) +
		KEY_SIZE +
		SIGNATURE_PUBLIC_KEY_SIZE +
		sizeof(int32_t);

	if (message.Size() < minMessageLength) {
		SendResponse(ERROR_TOO_SHORT, CowBuffer<uint8_t>());
		return;
	}

	const uint8_t *key = message.Pointer() + sizeof(int32_t);
	const uint8_t *signature = message.Pointer() +
		sizeof(int32_t) + KEY_SIZE;

	int32_t nameSize;
	memcpy(
		&nameSize,
		message.Pointer() + sizeof(int32_t) + KEY_SIZE +
		SIGNATURE_PUBLIC_KEY_SIZE,
		sizeof(int32_t));

	String name;

	if (nameSize) {
		bool messageCorrect =
			message.Size() >=
			sizeof(int32_t) * 2 + KEY_SIZE +
			SIGNATURE_PUBLIC_KEY_SIZE + nameSize;

		if (!messageCorrect) {
			SendResponse(ERROR_TOO_SHORT, CowBuffer<uint8_t>());
			return;
		}

		int indexBase = sizeof(int32_t) * 2 + KEY_SIZE +
			SIGNATURE_PUBLIC_KEY_SIZE;

		for (int i = 0; i < nameSize; i++) {
			name += message[i + indexBase];
		}
	}

	if (Users->HasUser(key)) {
		SendResponse(ERROR_USER_EXISTS, CowBuffer<uint8_t>());
		return;
	}

	Users->AddUser(key, signature, GetUnixTime(), name);

	SendResponse(OK, CowBuffer<uint8_t>());

	Log("Added user " + name + " with key " +
		DataToHex(key, KEY_SIZE) + ".");
}

void ControlSession::ProcessRemoveUserCommand(const CowBuffer<uint8_t> message)
{
	if (message.Size() != sizeof(int32_t) + KEY_SIZE) {
		SendResponse(ERROR_INVALID_SIZE, CowBuffer<uint8_t>());
		return;
	}

	const uint8_t *key = message.Pointer() + sizeof(int32_t);

	bool validUser = Users->HasUser(key);

	if (!validUser) {
		SendResponse(ERROR_INVALID_USER, CowBuffer<uint8_t>());
		return;
	}

	Users->RemoveUser(key);

	SendResponse(OK, CowBuffer<uint8_t>());

	Log("Removed user with key " +
		DataToHex(key, KEY_SIZE) + ".");
}

void ControlSession::ProcessListUsersCommand()
{
	const int32_t entrySize = KEY_SIZE + 55;

	CowBuffer<const uint8_t*> userKeys = Users->ListUsers();
	int32_t userCount = userKeys.Size();

	CowBuffer<uint8_t> message(sizeof(int32_t) + entrySize * userCount);
	memcpy(message.Pointer(), &userCount, sizeof(userCount));

	for (int32_t i = 0; i < userCount; i++) {
		const uint8_t *key = userKeys[i];

		memcpy(
			message.Pointer() + sizeof(int32_t) + entrySize * i,
			key,
			KEY_SIZE);

		String name = Users->GetUserName(key);
		memcpy(
			message.Pointer() + sizeof(int32_t) + entrySize * i +
			KEY_SIZE,
			name.CStr(),
			name.Length() + 1);
	}

	SendResponse(OK, message);
}

void ControlSession::ProcessUnknownCommand()
{
	SendResponse(ERROR_UNKNOWN_COMMAND, CowBuffer<uint8_t>());
}
