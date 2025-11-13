#include "ControlSession.hpp"

#include "../ServerCtl/SocketName.hpp"
#include "../Common/UnixTime.hpp"

bool ControlSession::TimePassed()
{
	int64_t t = GetUnixTime();

	if (t - Time > 10) {
		return false;
	}

	return true;
}

// Command structure.
// | Command id (int32) |
// |        stop        |
// |      add user      | key | signature | name size (int32) | name |
// |    remove user     | key |
bool ControlSession::Process()
{
	CowBuffer<uint8_t> message = Receive();

	if (message.Size() < sizeof(int32_t)) {
		return false;
	}

	int32_t command;
	memcpy(&command, message.Pointer(), sizeof(int32_t));

	switch (command) {
	case COMMAND_SHUTDOWN:
		ProcessShutdownCommand();
		break;
	case COMMAND_ADD_USER:
		ProcessAddUserCommand(message);
		break;
	case COMMAND_REMOVE_USER:
		ProcessRemoveUserCommand(message);
		break;
	default:
		ProcessUnknownCommand();
		break;
	}

	return true;
}

void ControlSession::SendResponse(int32_t value)
{
	CowBuffer<uint8_t> message(sizeof(value));
	memcpy(message.Pointer(), &value, sizeof(value));
	Send(message);
}

// |        stop        |
void ControlSession::ProcessShutdownCommand()
{
	*Work = false;
}

// |      add user      | key | signature | name size (int32) | name |
void ControlSession::ProcessAddUserCommand(CowBuffer<uint8_t> message)
{
	uint32_t minMessageLength =
		sizeof(int32_t) +
		KEY_SIZE +
		SIGNATURE_PUBLIC_KEY_SIZE +
		sizeof(int32_t);

	if (message.Size() < minMessageLength) {
		SendResponse(ERROR_TOO_SHORT);
		return;
	}

	const uint8_t *key = message.Pointer() + sizeof(int32_t);
	const uint8_t *signature = message.Pointer() + sizeof(int32_t) +
		KEY_SIZE;

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
			SendResponse(ERROR_TOO_SHORT);
			return;
		}

		int indexBase = sizeof(int32_t) * 2 + KEY_SIZE +
			SIGNATURE_PUBLIC_KEY_SIZE;

		for (int i = 0; i < nameSize; i++) {
			name += message[i + indexBase];
		}
	}

	Users->AddUser(key, signature, GetUnixTime(), name);

	SendResponse(OK);
}

void ControlSession::ProcessRemoveUserCommand(CowBuffer<uint8_t> message)
{
}

void ControlSession::ProcessUnknownCommand()
{
	SendResponse(ERROR_UNKNOWN_COMMAND);
}
