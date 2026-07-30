#ifndef _ENDIANNESS_HPP
#define _ENDIANNESS_HPP

#include <cstring>
#include <cstdint>

template<typename T>
T ChangeEndianness(T value)
{
	uint8_t buffer[sizeof(T)];
	memcpy(buffer, &value, sizeof(T));

	for (unsigned int i = 0; i < sizeof(T) / 2; i++) {
		uint8_t tmp = buffer[i];
		buffer[i] = buffer[sizeof(T) - 1 - i];
		buffer[sizeof(T) - 1 - i] = tmp;
	}

	memcpy(&value, buffer, sizeof(T));
	return value;
}

template<typename T>
T SetProtoEndian(T value)
{
#if defined(ENDIANNESS_LITTLE_ENDIAN)
	return value;
#elif defined(ENDIANNESS_BIG_ENDIAN)
	return ChangeEndianness(value);
#else
	#error Endianness is not defined.
#endif
}

#endif
