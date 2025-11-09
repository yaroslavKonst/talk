#include "Client.hpp"

int main(int argc, char **argv)
{
	try {
		Client client;
		return client.Run();
	}
	catch (Exception &ex) {
		printf("%s\n", ex.What().CStr());
	}

	return 100;
}
