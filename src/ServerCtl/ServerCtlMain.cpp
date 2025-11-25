#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cstring>
#include <cstdio>

#include "../Protocol/Session.hpp"
#include "../Common/Exception.hpp"
#include "../Common/MyString.hpp"
#include "../Common/Hex.hpp"
#include "../Common/Version.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

#include "SocketName.hpp"

static const char *ShutdownCommand = "shutdown";
static const char *GetKeyCommand = "getkey";
static const char *AddUserCommand = "adduser";
static const char *RemoveUserCommand = "removeuser";
static const char *ListUsersCommand = "listusers";

static int OpenSocket()
{
	const char *name = TALKD_SOCKET_NAME;

	int fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (fd == -1) {
		printf("Failed to create socket.\n");
		return -1;
	}

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, name, sizeof(addr.sun_path) - 1);

	int res = connect(fd, (struct sockaddr*)&addr, sizeof(addr));

	if (res == -1) {
		close(fd);
		printf("Failed to connect to talkd.\n");
		return -1;
	}

	return fd;
}

static CowBuffer<uint8_t> RequestStop()
{
	CowBuffer<uint8_t> result(sizeof(int32_t));
	*result.SwitchType<int32_t>() = COMMAND_SHUTDOWN;

	return result;
}

static CowBuffer<uint8_t> RequestGetKey()
{
	CowBuffer<uint8_t> result(sizeof(int32_t));
	*result.SwitchType<int32_t>() = COMMAND_GET_PUBLIC_KEY;

	return result;
}

static CowBuffer<uint8_t> RequestAddUser(int argc, char **argv)
{
	if (argc != 5) {
		printf("Usage: %s name key signature\n", AddUserCommand);
		return CowBuffer<uint8_t>();
	}

	String name(argv[2]);
	String keyHex(argv[3]);
	String signatureHex(argv[4]);

	if (name.Length() == 0) {
		printf("Name is empty.\n");
		return CowBuffer<uint8_t>();
	}

	if (keyHex.Length() != KEY_SIZE * 2) {
		printf("Key length is not equal to %d.\n", KEY_SIZE * 2);
		return CowBuffer<uint8_t>();
	}

	if (signatureHex.Length() != SIGNATURE_PUBLIC_KEY_SIZE * 2) {
		printf("Signature length is not equal to %d.\n",
			SIGNATURE_PUBLIC_KEY_SIZE * 2);
		return CowBuffer<uint8_t>();
	}

	CowBuffer<uint8_t> resultBuffer(
		sizeof(int32_t) * 2 +
		KEY_SIZE +
		SIGNATURE_PUBLIC_KEY_SIZE +
		name.Length());

	int32_t *command = resultBuffer.SwitchType<int32_t>();
	uint8_t *key = resultBuffer.Pointer(sizeof(int32_t));
	uint8_t *signature = resultBuffer.Pointer(
		sizeof(int32_t) + KEY_SIZE);
	int32_t *nameLength = resultBuffer.SwitchType<int32_t>(
		sizeof(int32_t) + KEY_SIZE +
		SIGNATURE_PUBLIC_KEY_SIZE);
	uint8_t *nameBuffer = resultBuffer.Pointer(
		sizeof(int32_t) * 2 + KEY_SIZE +
		SIGNATURE_PUBLIC_KEY_SIZE);

	*command = COMMAND_ADD_USER;
	*nameLength = name.Length();
	memcpy(nameBuffer, name.CStr(), *nameLength);
	HexToData(keyHex, key);
	HexToData(signatureHex, signature);

	return resultBuffer;
}

static CowBuffer<uint8_t> RequestRemoveUser(int argc, char **argv)
{
	if (argc != 3) {
		printf("Usage: %s key\n", AddUserCommand);
		return CowBuffer<uint8_t>();
	}

	String keyHex(argv[2]);

	if (keyHex.Length() != KEY_SIZE * 2) {
		printf("Key length is not equal to %d.\n", KEY_SIZE * 2);
		return CowBuffer<uint8_t>();
	}

	CowBuffer<uint8_t> resultBuffer(sizeof(int32_t) + KEY_SIZE);

	int32_t *command = resultBuffer.SwitchType<int32_t>();
	uint8_t *key = resultBuffer.Pointer(sizeof(int32_t));

	*command = COMMAND_REMOVE_USER;
	HexToData(keyHex, key);

	return resultBuffer;
}

static CowBuffer<uint8_t> RequestListUsers()
{
	CowBuffer<uint8_t> result(sizeof(int32_t));
	*result.SwitchType<int32_t>() = COMMAND_LIST_USERS;

	return result;
}

static CowBuffer<uint8_t> CreateRequest(int argc, char **argv)
{
	CowBuffer<uint8_t> request;

	if (!strcmp(argv[1], ShutdownCommand)) {
		request = RequestStop();
	} else if (!strcmp(argv[1], GetKeyCommand)) {
		request = RequestGetKey();
	} else if (!strcmp(argv[1], AddUserCommand)) {
		request = RequestAddUser(argc, argv);
	} else if (!strcmp(argv[1], RemoveUserCommand)) {
		request = RequestRemoveUser(argc, argv);
	} else if (!strcmp(argv[1], ListUsersCommand)) {
		request = RequestListUsers();
	}

	return request;
}

static void PrintError(int32_t code)
{
	switch (code) {
	case ERROR:
		printf("Error.\n");
		break;
	case ERROR_UNKNOWN_COMMAND:
		printf("Request is not supported by server.\n");
		break;
	case ERROR_TOO_SHORT:
		printf("Request is too short.\n");
		break;
	case ERROR_INVALID_SIZE:
		printf("Request has invalid size.\n");
		break;
	case ERROR_INVALID_USER:
		printf("Requested user does not exist.\n");
		break;
	case ERROR_USER_EXISTS:
		printf("User already exists.\n");
		break;
	default:
		printf("Unknown error code.\n");
		break;
	}
}

static void ProcessGetKey(const CowBuffer<uint8_t> response)
{
	if (response.Size() != KEY_SIZE) {
		printf("Invalid response length.\n");
		return;
	}

	String keyHex = DataToHex(response.Pointer(), KEY_SIZE);

	printf("%s\n", keyHex.CStr());
}

static void ProcessListUsers(const CowBuffer<uint8_t> response)
{
	const int32_t entrySize = KEY_SIZE + 55;
	int32_t userCount;

	if (response.Size() < sizeof(userCount)) {
		printf("Response does not contain user count.\n");
		return;
	}

	userCount = *response.SwitchType<int32_t>();

	for (int i = 0; i < userCount; i++) {
		String name(response.SwitchType<char>(
			sizeof(userCount) + entrySize * i + KEY_SIZE));
		String key = DataToHex(
			response.Pointer(sizeof(userCount) + entrySize * i),
			KEY_SIZE);

		printf("%s\n%s\n\n", name.CStr(), key.CStr());
	}
}

static int ProcessResponse(
	const char *command,
	CowBuffer<uint8_t> response)
{
	int32_t code;

	if (response.Size() < sizeof(code)) {
		printf("Response size is too small to contain result code.\n");
		return 1;
	}

	code = *response.SwitchType<int32_t>();

	if (code != OK) {
		PrintError(code);
		return 1;
	}

	if (response.Size() <= sizeof(code)) {
		return 0;
	}

	response = response.Slice(
		sizeof(code),
		response.Size() - sizeof(code));

	if (!strcmp(command, ShutdownCommand)) {
		printf("%s should not have response.\n", ShutdownCommand);
	} else if (!strcmp(command, GetKeyCommand)) {
		ProcessGetKey(response);
	} else if (!strcmp(command, AddUserCommand)) {
		printf("%s should not have response.\n", AddUserCommand);
	} else if (!strcmp(command, RemoveUserCommand)) {
		printf("%s should not have response.\n", RemoveUserCommand);
	} else if (!strcmp(command, ListUsersCommand)) {
		ProcessListUsers(response);
	}

	return 0;
}

static CowBuffer<uint8_t> SendRequest(const CowBuffer<uint8_t> command)
{
	Session session;
	session.SetInputSizeLimit(1024 * 1024 * 1024);
	session.Socket = OpenSocket();

	if (session.Socket == -1) {
		return CowBuffer<uint8_t>();
	}

	session.Send(command);

	bool res;

	while (session.CanWrite()) {
		res = session.Write();

		if (!res) {
			printf("Failed to send request.\n");
			return CowBuffer<uint8_t>();
		}
	}

	res = session.Read();

	if (!res) {
		return CowBuffer<uint8_t>();
	}

	while (session.Input) {
		res = session.Read();

		if (!res) {
			return CowBuffer<uint8_t>();
		}
	}

	return session.Receive();
}

static void PrintHelp()
{
	printf("Commands:\n");
	printf("\t%s\n", ShutdownCommand);
	printf("\t%s\n", GetKeyCommand);
	printf("\t%s\n", AddUserCommand);
	printf("\t%s\n", RemoveUserCommand);
	printf("\t%s\n", ListUsersCommand);
}

int main(int argc, char **argv)
{
	PrintVersionAndExit(argc, argv);

	if (argc < 2) {
		PrintHelp();
		return 1;
	}

	CowBuffer<uint8_t> request = CreateRequest(argc, argv);

	if (request.Size() == 0) {
		printf("Failed to create request.\n");
		return 1;
	}

	CowBuffer<uint8_t> response = SendRequest(request);

	if (response.Size() == 0) {
		printf("No response from server.\n");
		return 0;
	}

	return ProcessResponse(argv[1], response);
}
