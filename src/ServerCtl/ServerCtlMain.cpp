#include <cstdint>
#include <cstdio>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "RequestBuilder.hpp"
#include "ResponseProcessor.hpp"
#include "../Protocol/ControlParser.hpp"
#include "../Common/Version.hpp"
#include "../Common/Exception.hpp"
#include "../Common/StreamReader.hpp"
#include "../Common/StreamWriter.hpp"

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
	int socket = OpenSocket();

	if (socket == -1) {
		return CowBuffer<uint8_t>();
	}

	CowBuffer<uint8_t> commandSize(sizeof(uint64_t));
	*commandSize.SwitchType<uint64_t>() = command.Size();

	StreamWriter *writer = new StreamWriter(
		socket,
		commandSize.Concat(command));

	do {
		bool writeSuccess = writer->Write();

		if (!writeSuccess) {
			printf("Failed to send request.\n");
			return CowBuffer<uint8_t>();
		}
	} while (!writer->WritingEnd());

	delete writer;

	StreamReader *reader = new StreamReader(socket, sizeof(uint64_t));

	do {
		bool readSuccess = reader->Read();

		if (!readSuccess) {
			printf("Failed to get response size.\n");
			return CowBuffer<uint8_t>();
		}
	} while (!reader->ReadingEnd());

	uint64_t responseSize = *reader->GetBuffer().SwitchType<uint64_t>();
	delete reader;

	reader = new StreamReader(socket, responseSize);

	do {
		bool readSuccess = reader->Read();

		if (!readSuccess) {
			printf("Failed to get response.\n");
			return CowBuffer<uint8_t>();
		}
	} while (!reader->ReadingEnd());

	CowBuffer<uint8_t> response = reader->GetBuffer();
	delete reader;

	return response;
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
