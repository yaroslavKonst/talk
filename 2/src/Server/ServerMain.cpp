#include <stdio.h>

#include "Server.hpp"

static void Daemonize()
{
}

int main(int argc, char **argv)
{
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
