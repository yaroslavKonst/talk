#include <cstdint>
#include <cstdio>
#include <cstring>

int main()
{
	uint32_t value = 0x01020304;

	uint8_t buffer[sizeof(value)];

	memcpy(buffer, &value, sizeof(value));

	bool isLE =
		buffer[0] == 0x04 &&
		buffer[1] == 0x03 &&
		buffer[2] == 0x02 &&
		buffer[3] == 0x01;

	bool isBE =
		buffer[0] == 0x01 &&
		buffer[1] == 0x02 &&
		buffer[2] == 0x03 &&
		buffer[3] == 0x04;

	if (isLE) {
		printf("ENDIANNESS_LITTLE_ENDIAN\n");
		return 0;
	}

	if (isBE) {
		printf("ENDIANNESS_BIG_ENDIAN\n");
		return 0;
	}

	return 1;
}
