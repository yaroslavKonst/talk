#include "RequestBuilder.hpp"

#include <cstdio>
#include <arpa/inet.h>

#include "../Protocol/ControlParser.hpp"
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
	printf("Keys:\n");
	printf("  -help      Print help and exit.\n");
	printf("  -version   Print version and exit.\n");
	printf("  --         Treat all subsequent words as positional\n");
	printf("             arguments, stop key parsing beyond this key.\n");
	printf("\n");
	printf("Commands:\n");
	printf("  %s\n", ShutdownCommand);
	printf("  %s\n", GetKeyCommand);
	printf("  %s\n\n", ReloadCommand);

	printf("  %s\n", UserSection);
	printf("    %s\n", AddUserCommand);
	printf("    %s\n", RemoveUserCommand);
	printf("    %s [key]\n\n", ListUsersCommand);

	printf("  %s\n", IPSection);
	printf("    %s\n", ListBannedIPCommand);
	printf("    %s\n", BanIPCommand);
	printf("    %s\n", UnbanIPCommand);
}

void PrintShortHelp()
{
	printf("Use '-help' to get command list.\n");
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

static CowBuffer<uint8_t> RequestAddUser(const CowBuffer<String> args)
{
	if (args.Size() != 5) {
		printf(
			"Usage: %s %s NAME KEY\n",
			UserSection,
			AddUserCommand);
		THROW("Invalid number of arguments.");
	}

	String name = args[3];
	String keyHex = args[4];

	if (name.Length() == 0) {
		THROW("Name is empty.\n");
	}

	if (keyHex.Length() != Crypto::X25519::KEY_SIZE * 2) {
		THROW("Key length is not equal to " +
			ToString(Crypto::X25519::KEY_SIZE * 2) + ".");
	}

	CowBuffer<uint8_t> key(Crypto::X25519::KEY_SIZE);
	HexToData(keyHex, key.Pointer());

	CommandAddUser::Request request;
	request.Name = name;
	request.Key = key.Pointer();

	CowBuffer<uint8_t> resultBuffer = CommandAddUser::BuildRequest(request);

	return resultBuffer;
}

static CowBuffer<uint8_t> RequestRemoveUser(const CowBuffer<String> args)
{
	if (args.Size() != 4) {
		printf("Usage: %s %s NAME\n", UserSection, RemoveUserCommand);
		THROW("Invalid number of arguments.");
	}

	String name = args[3];

	if (name.Length() == 0) {
		THROW("Name is empty.");
	}

	CommandRemoveUser::Request request;
	request.Name = name;
	CowBuffer<uint8_t> resultBuffer = CommandRemoveUser::BuildRequest(
		request);

	return resultBuffer;
}

static CowBuffer<uint8_t> RequestListUsers(const CowBuffer<String> args)
{
	CommandListUsers::Request request;
	request.Flags = 0;

	for (unsigned int i = 3; i < args.Size(); i++) {
		if (args[i] == "key") {
			request.Flags |= CommandListUsers::ShowKeys;
		} else {
			THROW("Unknown option: " + args[i] + ".");
		}
	}

	CowBuffer<uint8_t> result = CommandListUsers::BuildRequest(request);
	return result;
}

static CowBuffer<uint8_t> RequestListBannedIP()
{
	CowBuffer<uint8_t> result(sizeof(int32_t));
	*result.SwitchType<int32_t>() = COMMAND_FAILBAN_LIST_BANNED;

	return result;
}

static CowBuffer<uint8_t> RequestBanIP(const CowBuffer<String> args)
{
	if (args.Size() != 4) {
		printf("Usage: %s %s IP\n", IPSection, BanIPCommand);
		THROW("Invalid number of arguments.");
	}

	CommandFailBanBan::Request request;
	bool res = request.IP.ParseIPAddress(args[3]);

	if (!res) {
		THROW("Invalid IP address format.");
	}

	return CommandFailBanBan::BuildRequest(request);
}

static CowBuffer<uint8_t> RequestUnbanIP(const CowBuffer<String> args)
{
	if (args.Size() != 4) {
		printf("Usage: %s %s IP\n", IPSection, UnbanIPCommand);
		THROW("Invalid number of arguments.");
	}

	CommandFailBanUnban::Request request;
	bool res = request.IP.ParseIPAddress(args[3]);

	if (!res) {
		THROW("Invalid IP address format.");
	}

	return CommandFailBanUnban::BuildRequest(request);
}

static CowBuffer<uint8_t> RequestReload()
{
	CowBuffer<uint8_t> result(sizeof(int32_t));
	*result.SwitchType<int32_t>() = COMMAND_RELOAD_CONFIG;

	return result;
}

CowBuffer<uint8_t> CreateRequestUser(const CowBuffer<String> args)
{
	if (args.Size() < 3) {
		PrintShortHelp();
		THROW("Not enough arguments.");
	}

	if (args[2] == AddUserCommand) {
		return RequestAddUser(args);
	} else if (args[2] == RemoveUserCommand) {
		return RequestRemoveUser(args);
	} else if (args[2] == ListUsersCommand) {
		return RequestListUsers(args);
	}

	THROW(args[2] + ": unknown command.");
}

CowBuffer<uint8_t> CreateRequestIp(const CowBuffer<String> args)
{
	if (args.Size() < 3) {
		PrintShortHelp();
		THROW("Not enough arguments.");
	}

	if (args[2] == ListBannedIPCommand) {
		return RequestListBannedIP();
	} else if (args[2] == BanIPCommand) {
		return RequestBanIP(args);
	} else if (args[2] == UnbanIPCommand) {
		return RequestUnbanIP(args);
	}

	THROW(args[2] + ": unknown command.");
}

CowBuffer<uint8_t> CreateRequest(const CowBuffer<String> args)
{
	if (args.Size() < 2) {
		PrintShortHelp();
		THROW("Not enough arguments.");
	}

	if (args[1] == ShutdownCommand) {
		return RequestShutdown();
	} else if (args[1] == GetKeyCommand) {
		return RequestGetKey();
	} else if (args[1] == ReloadCommand) {
		return RequestReload();
	} else if (args[1] == UserSection) {
		return CreateRequestUser(args);
	} else if (args[1] == IPSection) {
		return CreateRequestIp(args);
	}

	THROW(args[1] + ": unknown command.");
}
