#include <cstdio>

#include "Client.hpp"
#include "../Common/Exception.hpp"
#include "../Common/Version.hpp"
#include "../Common/CommandLineParser.hpp"

static const char *HelpKey = "-help";
static const char *VersionKey = "-version";

static void PrintHelp()
{
	printf("%-19s print help and exit.\n", HelpKey);
	printf("%-19s print version and exit.\n", VersionKey);
}

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

	const CowBuffer<String> posArgs = cmd.GetPositionalArguments();

	if (posArgs.Size() > 1) {
		printf("Unknown argument: %s.\n", posArgs[1].CStr());
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
			printf("Use '-help' to get argument list.\n");
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

		Client client;
		return client.Run();
	}
	catch (Exception &ex) {
		printf("%s\n", ex.Message().CStr());
	}

	return 100;
}
