#include "ResponseProcessor.hpp"

#include <cstdio>
#include <arpa/inet.h>

#include "../Protocol/ControlParser.hpp"
#include "../Common/MyString.hpp"
#include "../Common/Hex.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

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
	case ERROR_USER_BUSY:
		printf("User is busy. Suspend the account before removal.\n");
		break;
	case ERROR_INVALID_IP:
		printf("Invalid IP address.\n");
		break;
	case ERROR_UNSUPPORTED_OPTION:
		printf("Unsupported option.\n");
		break;
	default:
		printf("Unknown error code.\n");
		break;
	}
}

static int ProcessResultCode(const CowBuffer<uint8_t> response)
{
	int32_t code;

	if (response.Size() != sizeof(code)) {
		printf("Invalid response length.\n");
		return 1;
	}

	code = *response.SwitchType<int32_t>();

	if (code != OK) {
		PrintError(code);
		return 1;
	}

	return 0;
}

static int ProcessShutdown(const CowBuffer<uint8_t> response)
{
	if (!response.Size()) {
		return 0;
	}

	printf("Command should not have response.\n");
	return 1;
}

static int ProcessGetKey(const CowBuffer<uint8_t> response)
{
	int32_t code;

	if (response.Size() != sizeof(code) + Crypto::X25519::KEY_SIZE) {
		printf("Invalid response length.\n");
		return 1;
	}

	code = *response.SwitchType<int32_t>();

	if (code != OK) {
		PrintError(code);
		return 1;
	}

	String keyHex = DataToHex(
		response.Pointer(sizeof(code)),
		Crypto::X25519::KEY_SIZE);
	printf("%s\n", keyHex.CStr());
	return 0;
}

static int ProcessListUsers(CowBuffer<uint8_t> response)
{
	CommandListUsers::Response data;
	bool parseResult = CommandListUsers::ParseResponse(response, data);

	if (!parseResult) {
		printf("Response has invalid format. Parsing failed.\n");
		return 1;
	}

	if (data.Code != OK) {
		PrintError(data.Code);
		return 1;
	}

	for (unsigned int i = 0; i < data.Data.Size(); i++) {
		printf("%s\n", data.Data[i].Name.CStr());

		if (data.Flags) {
			if (data.Flags & CommandListUsers::ShowKeys) {
				String keyHex =
					DataToHex(
						data.Data[i].Key.Key,
						Crypto::X25519::KEY_SIZE);
				printf("%s\n", keyHex.CStr());
			}

			printf("\n");
		}
	}

	return 0;
}

static int ProcessListBannedIP(CowBuffer<uint8_t> response)
{
	CommandFailBanListBanned::Response resp;
	bool parseResult = CommandFailBanListBanned::ParseResponse(
		response,
		resp);

	if (!parseResult) {
		printf("Response is invalid.\n");
		return 1;
	}

	if (resp.Code != OK) {
		PrintError(resp.Code);
		return 2;
	}

	for (unsigned int i = 0; i < resp.BannedIPList.Size(); i++) {
		printf("%s\n", resp.BannedIPList[i].ToString().CStr());
	}

	return 0;
}

int ProcessResponse(
	int32_t commandId,
	CowBuffer<uint8_t> response)
{
	if (commandId == COMMAND_SHUTDOWN) {
		return ProcessShutdown(response);
	} else if (commandId == COMMAND_GET_PUBLIC_KEY) {
		return ProcessGetKey(response);
	} else if (commandId == COMMAND_ADD_USER) {
		return ProcessResultCode(response);
	} else if (commandId == COMMAND_REMOVE_USER) {
		return ProcessResultCode(response);
	} else if (commandId == COMMAND_LIST_USERS) {
		return ProcessListUsers(response);
	} else if (commandId == COMMAND_FAILBAN_LIST_BANNED) {
		return ProcessListBannedIP(response);
	} else if (commandId == COMMAND_FAILBAN_BAN) {
		return ProcessResultCode(response);
	} else if (commandId == COMMAND_FAILBAN_UNBAN) {
		return ProcessResultCode(response);
	} else if (commandId == COMMAND_RELOAD_CONFIG) {
		return ProcessResultCode(response);
	}

	printf("Unknown command.\n");
	return 10;
}
