#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "Server.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/File.hpp"
#include "../Common/Version.hpp"

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

int main(int argc, char **argv)
{
	PrintVersionAndExit(argc, argv);

	try {
		if (argc != 1) {
			return 1;
		}

		Server server;
		Daemonize();
		return server.Run();
	}
	catch (Exception &ex) {
		printf("%s\n", ex.What().CStr());
	}

	return 100;
}
