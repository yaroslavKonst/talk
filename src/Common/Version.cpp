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
