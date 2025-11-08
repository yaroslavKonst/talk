#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cstring>
#include <cstdio>

#include "../Crypto/Crypto.hpp"
#include "../Common/Exception.hpp"
#include "../Common/MyString.hpp"
#include "../Common/Hex.hpp"

#include "SocketName.hpp"

static int OpenSocket()
{
	const char *name = TALKD_SOCKET_NAME;

	int fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (fd == -1) {
		THROW("Failed to create socket.");
	}

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, name, sizeof(addr.sun_path) - 1);

	int res = connect(fd, (struct sockaddr*)&addr, sizeof(addr));

	if (res == -1) {
		close(fd);
		THROW("Failed to connect to talkd.");
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

static int ProcessAdduser(int argc, char **argv)
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
		return 1;
	}

	signatureHex = GetKey("User signature: ", SIGNATURE_PUBLIC_KEY_SIZE);

	if (signatureHex.Length() == 0) {
		return 1;
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
		return 1;
	}

	int sockFd = OpenSocket();

	int32_t command = ADD_USER;

	res = write(sockFd, &command, sizeof(command));

	if (res != sizeof(command)) {
		printf("Failed to send command.\n");
		shutdown(sockFd, SHUT_RDWR);
		close(sockFd);
		return 1;
	}

	command = name.Length();

	res = write(sockFd, &command, sizeof(command));

	if (res != sizeof(command)) {
		printf("Failed to send command.\n");
		shutdown(sockFd, SHUT_RDWR);
		close(sockFd);
		return 1;
	}

	res = write(sockFd, name.CStr(), name.Length());

	if (res != name.Length()) {
		printf("Failed to send command.\n");
		shutdown(sockFd, SHUT_RDWR);
		close(sockFd);
		return 1;
	}

	uint8_t data[KEY_SIZE];

	HexToData(keyHex, data);

	res = write(sockFd, data, KEY_SIZE);

	if (res != KEY_SIZE) {
		printf("Failed to send command.\n");
		shutdown(sockFd, SHUT_RDWR);
		close(sockFd);
		return 1;
	}

	HexToData(signatureHex, data);

	res = write(sockFd, data, SIGNATURE_PUBLIC_KEY_SIZE);

	if (res != SIGNATURE_PUBLIC_KEY_SIZE) {
		printf("Failed to send command.\n");
		shutdown(sockFd, SHUT_RDWR);
		close(sockFd);
		return 1;
	}

	res = read(sockFd, &command, sizeof(command));

	if (res != sizeof(command)) {
		printf("Failed to get response.\n");
		shutdown(sockFd, SHUT_RDWR);
		close(sockFd);
		return 1;
	}

	shutdown(sockFd, SHUT_RDWR);
	close(sockFd);

	if (command == OK) {
		printf("User added.\n");
		return 0;
	}

	printf("Failed to add user.");
	return 1;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		printf("No command given.\n");
		printf("Commands: adduser.\n");
		return 1;
	}

	if (!strcmp(argv[1], "adduser")) {
		return ProcessAdduser(argc, argv);
	}

	printf("Unknown command.\n");
	return 1;
}
