#include "RequestBuilder.hpp"

#include <cstdio>
#include <arpa/inet.h>

#include "SocketName.hpp"
#include "../Common/Exception.hpp"
#include "../Common/Hex.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

static const char *ShutdownCommand = "shutdown";
static const char *ReloadCommand = "reload";
static const char *GetKeyCommand = "getkey";

static const char *UserSection = "user";
static const char *AddUserCommand = "add";
static const char *RemoveUserCommand = "remove";
static const char *ListUsersCommand = "list";

static const char *IPSection = "ip";
static const char *ListBannedIPCommand = "listbanned";
static const char *BanIPCommand = "ban";
static const char *UnbanIPCommand = "unban";

void PrintHelp()
{
	printf("Commands:\n");
	printf("  %s\n", ShutdownCommand);
	printf("  %s\n", GetKeyCommand);
	printf("  %s\n\n", ReloadCommand);

	printf("  %s\n", UserSection);
	printf("    %s\n", AddUserCommand);
	printf("    %s\n", RemoveUserCommand);
	printf("    %s\n\n", ListUsersCommand);

	printf("  %s\n", IPSection);
	printf("    %s\n", ListBannedIPCommand);
	printf("    %s\n", BanIPCommand);
	printf("    %s\n", UnbanIPCommand);
}

void PrintShortHelp()
{
	printf("Use --help to get command list.\n");
}

static CowBuffer<uint8_t> RequestShutdown()
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
	if (argc != 6) {
		printf(
			"Usage: %s %s NAME KEY SIGNATURE\n",
			UserSection,
			AddUserCommand);
		THROW("Invalid number of arguments.");
	}

	String name(argv[3]);
	String keyHex(argv[4]);
	String signatureHex(argv[5]);

	if (name.Length() == 0) {
		THROW("Name is empty.\n");
	}

	if (keyHex.Length() != KEY_SIZE * 2) {
		THROW("Key length is not equal to " +
			ToString(KEY_SIZE * 2) + ".");
	}

	if (signatureHex.Length() != SIGNATURE_PUBLIC_KEY_SIZE * 2) {
		THROW("Signature length is not equal to " +
			ToString(SIGNATURE_PUBLIC_KEY_SIZE * 2) + ".");
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
	if (argc != 4) {
		printf("Usage: %s %s KEY\n", UserSection, RemoveUserCommand);
		THROW("Invalid number of arguments.");
	}

	String keyHex(argv[3]);

	if (keyHex.Length() != KEY_SIZE * 2) {
		THROW("Key length is not equal to " +
			ToString(KEY_SIZE * 2) + ".");
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

static CowBuffer<uint8_t> RequestListBannedIP()
{
	CowBuffer<uint8_t> result(sizeof(int32_t));
	*result.SwitchType<int32_t>() = COMMAND_LIST_BANNED_IP;

	return result;
}

static CowBuffer<uint8_t> RequestBanIP(int argc, char **argv)
{
	if (argc != 4) {
		printf("Usage: %s %s IP\n", IPSection, BanIPCommand);
		THROW("Invalid number of arguments.");
	}

	CowBuffer<uint8_t> result(sizeof(int32_t) + sizeof(uint32_t));
	*result.SwitchType<int32_t>() = COMMAND_BAN_IP;

	struct in_addr addr;

	int res = inet_aton(argv[3], &addr);

	if (!res) {
		THROW("Invalid IP address format.");
	}

	*result.SwitchType<uint32_t>(sizeof(int32_t)) = addr.s_addr;
	return result;
}

static CowBuffer<uint8_t> RequestUnbanIP(int argc, char **argv)
{
	if (argc != 4) {
		printf("Usage: %s %s IP\n", IPSection, UnbanIPCommand);
		THROW("Invalid number of arguments.");
	}

	CowBuffer<uint8_t> result(sizeof(int32_t) + sizeof(uint32_t));
	*result.SwitchType<int32_t>() = COMMAND_UNBAN_IP;

	struct in_addr addr;

	int res = inet_aton(argv[3], &addr);

	if (!res) {
		THROW("Invalid IP address format.");
	}

	*result.SwitchType<uint32_t>(sizeof(int32_t)) = addr.s_addr;
	return result;
}

static CowBuffer<uint8_t> RequestReload()
{
	CowBuffer<uint8_t> result(sizeof(int32_t));
	*result.SwitchType<int32_t>() = COMMAND_RELOAD;

	return result;
}

CowBuffer<uint8_t> CreateRequestUser(int argc, char **argv)
{
	if (argc < 3) {
		PrintShortHelp();
		THROW("Not enough arguments.");
	}

	if (!strcmp(argv[2], AddUserCommand)) {
		return RequestAddUser(argc, argv);
	} else if (!strcmp(argv[2], RemoveUserCommand)) {
		return RequestRemoveUser(argc, argv);
	} else if (!strcmp(argv[2], ListUsersCommand)) {
		return RequestListUsers();
	}

	THROW(String(argv[2]) + ": unknown command.");
}

CowBuffer<uint8_t> CreateRequestIp(int argc, char **argv)
{
	if (argc < 3) {
		PrintShortHelp();
		THROW("Not enough arguments.");
	}

	if (!strcmp(argv[2], ListBannedIPCommand)) {
		return RequestListBannedIP();
	} else if (!strcmp(argv[2], BanIPCommand)) {
		return RequestBanIP(argc, argv);
	} else if (!strcmp(argv[2], UnbanIPCommand)) {
		return RequestUnbanIP(argc, argv);
	}

	THROW(String(argv[2]) + ": unknown command.");
}

CowBuffer<uint8_t> CreateRequest(int argc, char **argv)
{
	if (argc < 2) {
		PrintShortHelp();
		THROW("Not enough arguments.");
	}

	if (!strcmp(argv[1], ShutdownCommand)) {
		return RequestShutdown();
	} else if (!strcmp(argv[1], GetKeyCommand)) {
		return RequestGetKey();
	} else if (!strcmp(argv[1], ReloadCommand)) {
		return RequestReload();
	} else if (!strcmp(argv[1], UserSection)) {
		return CreateRequestUser(argc, argv);
	} else if (!strcmp(argv[1], IPSection)) {
		return CreateRequestIp(argc, argv);
	}

	THROW(String(argv[1]) + ": unknown command.");
}
