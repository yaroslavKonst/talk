#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "Server.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/File.hpp"
#include "../Common/CommandLineParser.hpp"
#include "../Common/Version.hpp"
#include "../Common/Exception.hpp"
#include "../Common/Log.hpp"

static int OpenLog()
{
	CreateDirectory("logs");

	int64_t timestamp = GetUnixTime();

	String timeString = ctime(&timestamp);
	timeString = timeString.Substring(0, timeString.Length() - 1);

	String logName = "logs/Log_" +
		timeString.Replace(' ', '_').Replace(':', '-') + ".txt";

	int logFd = open(logName.CStr(), O_WRONLY | O_CREAT, 0600);

	if (logFd == -1) {
		THROW("Failed to open log.");
	}

	return logFd;
}

static int OpenDevNull()
{
	int nullFd = open("/dev/null", O_RDONLY);

	if (nullFd == -1) {
		THROW("Failed to open /dev/null.");
	}

	return nullFd;
}

static void ReplaceFds()
{
	int logFd = OpenLog();
	int nullFd = OpenDevNull();

	close(0);
	close(1);
	close(2);

	dup2(nullFd, 0);
	dup2(logFd, 1);
	dup2(logFd, 2);

	close(nullFd);
	close(logFd);
}

static void Wait(int pid)
{
	int status;
	int rpid = waitpid(pid, &status, 0);

	if (rpid != pid) {
		THROW("Failure on waitpid.");
	}

	if (WIFEXITED(status)) {
		int code = WEXITSTATUS(status);
		printf("Exit %d.\n", code);
	} else if (WIFSIGNALED(status)) {
		int sigcode = WTERMSIG(status);
		printf("Killed with signal %d.\n", sigcode);
	} else {
		printf("End.\n");
	}
}

static void Daemonize()
{
	ReplaceFds();

	if (fork()) {
		exit(0);
	}

	setsid();

	if (fork()) {
		exit(0);
	}

	int pid = fork();

	if (!pid) {
		return;
	}

	Wait(pid);
	exit(0);
}

static const char *HelpKey = "-help";
static const char *VersionKey = "-version";
static const char *NoDaemonizeKey = "-no-D";
static const char *NoMultilineLogKey = "-no-multiline-log";

static void PrintHelp()
{
	printf("%-19s print help and exit.\n", HelpKey);
	printf("%-19s print version and exit.\n", VersionKey);
	printf("%-19s do not daemonize.\n", NoDaemonizeKey);
	printf(
		"%-19s do not print log entries in multiline format.\n",
		NoMultilineLogKey);
}

static bool ParseCmd(CommandLineParser &cmd, int argc, char **argv)
{
	CowBuffer<String>keys(4);
	keys[0] = HelpKey;
	keys[1] = VersionKey;
	keys[2] = NoDaemonizeKey;
	keys[3] = NoMultilineLogKey;

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

		if (!cmd.GetKeyValue(NoMultilineLogKey)) {
			AllowMultilineLog(true);
		}

		Server server;

		if (!cmd.GetKeyValue(NoDaemonizeKey)) {
			Daemonize();
		}

		return server.Run();
	}
	catch (Exception &ex) {
		Log("Fatal", ex.Message());
	}

	return 100;
}
