#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cstring>
#include <cstdio>

#include "../Crypto/Crypto.hpp"
#include "../Common/Exception.hpp"
#include "../Common/MyString.hpp"
#include "../Common/CowBuffer.hpp"
#include "../Common/Hex.hpp"

#include "SocketName.hpp"

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

static String GetKey(const char *prompt, int length)
{
	String keyHex;
	char c;

	printf(prompt);
	fflush(stdout);

	while (read(0, &c, 1) == 1) {
		if (c == '\n') {
			break;
		}

		keyHex += c;
	}

	if (keyHex.Length() != length * 2) {
		printf("Invalid key size, it must be %d.\n", length * 2);
		return String();
	}

	for (int i = 0; i < keyHex.Length(); i++) {
		c = keyHex.CStr()[i];

		bool validChar =
			(c >= '0' && c <= '9') ||
			(c >= 'a' && c <= 'f');

		if (!validChar) {
			printf("Key contains invalid character.\n");
			return String();
		}
	}

	return keyHex;
}

static CowBuffer<uint8_t> ProcessStop()
{
	CowBuffer<uint8_t> result(sizeof(uint64_t) + sizeof(int32_t));

	uint64_t size = result.Size() - sizeof(uint64_t);
	int32_t command = COMMAND_SHUTDOWN;

	memcpy(result.Pointer(), &size, sizeof(size));
	memcpy(result.Pointer() + sizeof(size), &command, sizeof(command));

	return result;
}

static CowBuffer<uint8_t> ProcessAdduser(int argc, char **argv)
{
	String name;
	String keyHex;
	String signatureHex;

	char c;

	printf("User name: ");
	fflush(stdout);

	while (read(0, &c, 1) == 1) {
		if (c == '\n') {
			break;
		}

		name += c;
	}

	keyHex = GetKey("User key: ", KEY_SIZE);

	if (keyHex.Length() == 0) {
		return CowBuffer<uint8_t>();
	}

	signatureHex = GetKey("User signature: ", SIGNATURE_PUBLIC_KEY_SIZE);

	if (signatureHex.Length() == 0) {
		return CowBuffer<uint8_t>();
	}

	printf("User name: %s\nKey: %s\nSignature: %s\n",
		name.CStr(),
		keyHex.CStr(),
		signatureHex.CStr());
	printf("Correct? [y/N] ");
	fflush(stdout);

	c = 0;
	int res = read(0, &c, 1);

	if (res != 1 || c != 'y') {
		printf("Interrupt.\n");
		return CowBuffer<uint8_t>();
	}

	CowBuffer<uint8_t> resultBuffer(
		sizeof(uint64_t) +
		sizeof(int32_t) * 2 +
		KEY_SIZE +
		SIGNATURE_PUBLIC_KEY_SIZE +
		name.Length());

	int32_t *command = (int32_t*)(resultBuffer.Pointer() +
		sizeof(uint64_t));
	uint8_t *key = resultBuffer.Pointer() +
		sizeof(uint64_t) + sizeof(int32_t);
	uint8_t *signature = resultBuffer.Pointer() +
		sizeof(uint64_t) + sizeof(int32_t) + KEY_SIZE;
	int32_t *nameLength = (int32_t*)(resultBuffer.Pointer() +
		sizeof(uint64_t) + sizeof(int32_t) + KEY_SIZE +
		SIGNATURE_PUBLIC_KEY_SIZE);
	uint8_t *nameBuffer = resultBuffer.Pointer() +
		sizeof(uint64_t) + sizeof(int32_t) * 2 + KEY_SIZE +
		SIGNATURE_PUBLIC_KEY_SIZE;

	*command = COMMAND_ADD_USER;
	*nameLength = name.Length();
	memcpy(nameBuffer, name.CStr(), *nameLength);
	HexToData(keyHex, key);
	HexToData(signatureHex, signature);

	uint64_t bufLen = resultBuffer.Size() - sizeof(uint64_t);
	memcpy(resultBuffer.Pointer(), &bufLen, sizeof(bufLen));

	return resultBuffer;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		printf("No command given.\n");
		printf("Commands:\n\tshutdown\n\tadduser\n");
		return 1;
	}

	CowBuffer<uint8_t> command;

	if (!strcmp(argv[1], "adduser")) {
		command = ProcessAdduser(argc, argv);
	} else if (!strcmp(argv[1], "shutdown")) {
		command = ProcessStop();
	}

	if (command.Size() == 0) {
		printf("Invalid command.\n");
		return 1;
	}

	int fd = OpenSocket();

	if (fd == -1) {
		return 1;
	}

	int wrBytes = write(fd, command.Pointer(), command.Size());

	if (wrBytes != (int)command.Size()) {
		printf("Failed to send command.\n");
		return 2;
	}

	uint8_t response[sizeof(uint64_t) + sizeof(int32_t)];

	int rdBytes = read(fd, response, sizeof(uint64_t) + sizeof(int32_t));

	if (rdBytes != sizeof(uint64_t) + sizeof(int32_t)) {
		printf("No response from server.\n");
		return 0;
	}

	int32_t code;
	memcpy(&code, response + sizeof(uint64_t), sizeof(code));

	if (code == OK) {
		return 0;
	}

	if (code == ERROR_UNKNOWN_COMMAND) {
		printf("Command is not supported by server.\n");
		return 1;
	}

	if (code == ERROR_TOO_SHORT) {
		printf("Command is too short.\n");
		return 1;
	}

	printf("Unknown result.\n");
	return 1;
}
