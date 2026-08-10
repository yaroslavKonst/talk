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
#include "../Common/CommandLineParser.hpp"

static int OpenSocket()
{
	const char *name = TALKD_SOCKET_NAME;

	int fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (fd == -1) {
		THROW("Failed to create socket.");
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
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
			delete writer;
			THROW("Failed to send request.");
		}
	} while (!writer->WritingEnd());

	delete writer;

	StreamReader *reader = new StreamReader(socket, sizeof(uint64_t));

	do {
		bool readSuccess = reader->Read();

		if (!readSuccess) {
			delete reader;
			return CowBuffer<uint8_t>();
		}
	} while (!reader->ReadingEnd());

	uint64_t responseSize = *reader->GetBuffer().SwitchType<uint64_t>();
	delete reader;

	reader = new StreamReader(socket, responseSize);

	do {
		bool readSuccess = reader->Read();

		if (!readSuccess) {
			delete reader;
			THROW("Failed to get response.");
		}
	} while (!reader->ReadingEnd());

	CowBuffer<uint8_t> response = reader->GetBuffer();
	delete reader;

	return response;
}

static const char *HelpKey = "-help";
static const char *VersionKey = "-version";

static bool ParseCmd(CommandLineParser &cmd, int argc, char **argv)
{
	CowBuffer<String>keys(2);
	keys[0] = HelpKey;
	keys[1] = VersionKey;

	cmd = CommandLineParser(keys, CowBuffer<String>(), CowBuffer<String>());
	bool parseResult = cmd.Parse(argc, argv);

	if (!parseResult) {
		printf("%s\n", cmd.GetErrorString().CStr());
		return false;
	}

	return true;
}

int main(int argc, char **argv)
{
	try {
		CommandLineParser cmd;
		bool parseResult = ParseCmd(cmd, argc, argv);

		if (!parseResult) {
			PrintShortHelp();
			return 1;
		}

		if (cmd.GetKeyValue(HelpKey)) {
			PrintHelp();
			return 0;
		}

		if (cmd.GetKeyValue(VersionKey)) {
			PrintVersion();
			return 0;
		}

		if (cmd.GetPositionalArguments().Size() < 2) {
			PrintShortHelp();
			return 1;
		}

		CowBuffer<uint8_t> request =
			CreateRequest(cmd.GetPositionalArguments());

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
