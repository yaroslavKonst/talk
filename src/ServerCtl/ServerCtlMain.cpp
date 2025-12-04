#include <cstdint>
#include <cstdio>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "SocketName.hpp"
#include "RequestBuilder.hpp"
#include "ResponseProcessor.hpp"
#include "../Protocol/Session.hpp"
#include "../Common/Version.hpp"
#include "../Common/Exception.hpp"

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

static CowBuffer<uint8_t> SendRequest(const CowBuffer<uint8_t> command)
{
	Session session;
	session.InputSizeLimit = 1024 * 1024 * 1024;
	session.Socket = OpenSocket();

	if (session.Socket == -1) {
		return CowBuffer<uint8_t>();
	}

	session.Send(command, 0, false);

	bool res;

	while (session.CanWrite()) {
		res = session.Write();

		if (!res) {
			printf("Failed to send request.\n");
			return CowBuffer<uint8_t>();
		}
	}

	while (!session.CanReceive()) {
		res = session.Read();

		if (!res) {
			return CowBuffer<uint8_t>();
		}
	}

	return session.Receive();
}

int main(int argc, char **argv)
{
	try {
		PrintVersionAndExit(argc, argv);

		if (argc < 2) {
			PrintShortHelp();
			return 1;
		}

		if (!strcmp(argv[1], "--help")) {
			PrintHelp();
			return 0;
		}

		CowBuffer<uint8_t> request = CreateRequest(argc, argv);

		if (request.Size() == 0) {
			printf("Failed to create request.\n");
			return 1;
		}

		CowBuffer<uint8_t> response = SendRequest(request);

		int32_t commandId = *request.SwitchType<int32_t>();
		return ProcessResponse(commandId, response);
	} catch (Exception &ex) {
		printf("%s\n", ex.Message().CStr());
	}

	return 10;
}
