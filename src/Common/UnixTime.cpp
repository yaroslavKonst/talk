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

int64_t GetMonotonicMillisecondTime()
{
	struct timespec ts;

	int res = clock_gettime(CLOCK_MONOTONIC, &ts);

	if (res == -1) {
		THROW("Failed to get monotonic system time.");
	}

	// Converting seconds and nanoseconds to milliseconds before return.
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
