#ifndef _HEX_HPP
#define _HEX_HPP

#include "MyString.hpp"
#include "Exception.hpp"

inline void HexToData(String hex, uint8_t *data)
{
	if (hex.Length() % 2 != 0) {
		THROW("Hex string has odd length.");
	}

	int byteIndex = 0;
	uint8_t currentByte = 0;

	bool hasHalf = false;

	for (int i = 0; i < hex.Length(); i++) {
		char c = hex.CStr()[i];

		bool validChar =
			(c >= '0' && c <= '9') ||
			(c >= 'a' && c <= 'f');

		if (!validChar) {
			THROW("Hex string contains invalid character.");
		}

		bool isDigit = c >= '0' && c <= '9';
		int value = isDigit ? c - '0' : c - 'a' + 10;

		if (hasHalf) {
			currentByte |= value;
			data[byteIndex] = currentByte;

			++byteIndex;
			currentByte = 0;
			hasHalf = false;
		} else {
			currentByte = value << 4;
			hasHalf = true;
		}
	}
}

inline String DataToHex(const uint8_t *data, uint64_t size)
{
	String res;

	for (uint64_t i = 0; i < size; i++) {
		uint8_t hex[2];

		hex[0] = data[i] >> 4;
		hex[1] = data[i] & 0b1111;

		for (int c = 0; c < 2; c++) {
			if (hex[c] < 10) {
				res += '0' + hex[c];
			} else {
				res += 'a' + hex[c] - 10;
			}
		}
	}

	return res;
}

template <typename T>
String ToHex(T value)
{
	uint8_t data[sizeof(T)];

	for (uint32_t i = 0; i < sizeof(T); i++) {
		data[i] = ((const uint8_t*)&value)[sizeof(T) - 1 - i];
	}

	return DataToHex(data, sizeof(T));
}

template <typename T>
T HexToInt(String string)
{
	uint8_t data[sizeof(T)];
	HexToData(string, data);
	T value;

	for (uint32_t i = 0; i < sizeof(T); i++) {
		((uint8_t*)&value)[sizeof(T) - 1 - i] = data[i];
	}

	return value;
}

#endif
