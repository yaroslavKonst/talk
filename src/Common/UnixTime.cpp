#include "UnixTime.hpp"

#include <time.h>

#include "Exception.hpp"

int64_t GetUnixTime()
{
	int64_t val = time(nullptr);

	if (val == -1) {
		THROW("Failed to get system time.");
	}

	return val;
}
