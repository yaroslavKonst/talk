#include <time.h>
#include <cstdio>
#include <cstring>

#include "../src/Server/UserDB.hpp"
#include "../src/Common/UnixTime.hpp"

void TimingTest()
{
	UserDB db;

	for (int i = 1; i <= 100; i++) {
		uint8_t key[KEY_SIZE];
		memset(key, i, KEY_SIZE);
		db.AddUser(key, key, GetUnixTime(), "test user");
	}


	long averageExistingTime = 0;
	long maxExistingTime = 0;
	long minExistingTime = 0x7fffffffffffffff;
	long averageNotExistingTime = 0;
	long maxNotExistingTime = 0;
	long minNotExistingTime = 0x7fffffffffffffff;
	long iterationsExist = 0;
	long iterationsNotExist = 0;

	for (int iter = 0; iter < 10000; iter++) {
		printf("Iteration %d.\n", iter);

		for (int u = 0; u <= 255; u++) {
			uint8_t key[KEY_SIZE];
			memset(key, u, KEY_SIZE);

			struct timespec start;
			struct timespec end;

			clock_gettime(CLOCK_REALTIME, &start);
			bool exists = db.HasUser(key);
			clock_gettime(CLOCK_REALTIME, &end);

			long duration = end.tv_sec - start.tv_sec;
			duration *= 1000000000ull;
			duration += end.tv_nsec - start.tv_nsec;

			if (exists) {
				averageExistingTime += duration;

				if (duration > maxExistingTime) {
					maxExistingTime = duration;
				}

				if (duration < minExistingTime) {
					minExistingTime = duration;
				}

				++iterationsExist;
			} else {
				averageNotExistingTime += duration;

				if (duration > maxNotExistingTime) {
					maxNotExistingTime = duration;
				}

				if (duration < minNotExistingTime) {
					minNotExistingTime = duration;
				}

				++iterationsNotExist;
			}
		}
	}

	printf(
		"Existing: max %ld ns, min %ld ns, average %ld ns.\n",
		maxExistingTime,
		minExistingTime,
		averageExistingTime / iterationsExist);

	printf(
		"Not existing: max %ld ns, min %ld ns, average %ld ns.\n",
		maxNotExistingTime,
		minNotExistingTime,
		averageNotExistingTime / iterationsNotExist);

	unlink("talkd.users");
}

int main(int argc, char **argv)
{
	TimingTest();
	return 0;
}
