#include "Version.hpp"

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "../Version.hpp"

#define TO_STRING2(X) #X
#define TO_STRING(X) TO_STRING2(X)

void PrintVersion()
{
	printf("Version: %s.\n", TO_STRING(VERSION));
}

void PrintVersionAndExit(int argc, char **argv)
{
	if (argc != 2) {
		return;
	}

	if (strcmp(argv[1], "--version")) {
		return;
	}

	PrintVersion();
	exit(0);
}
