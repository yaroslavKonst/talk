#include "Client.hpp"

#include "../Common/Exception.hpp"
#include "../Common/Version.hpp"

int main(int argc, char **argv)
{
	PrintVersionAndExit(argc, argv);

	try {
		Client client;
		return client.Run();
	}
	catch (Exception &ex) {
		printf("%s\n", ex.What().CStr());
	}

	return 100;
}
