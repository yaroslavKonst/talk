#ifndef _HEX_HPP
#define _HEX_HPP

#include "MyString.hpp"
#include "Exception.hpp"

inline void HexToData(String hex, uint8_t *data)
{
	if (hex.Size() % 2 != 0) {
		THROW("Hex string has odd length.");
	}

	int byteIndex;
	uint8_t currentByte = 0;

	bool hasHalf = false;

	for (int i = 0; i < hex.Size(); i++) {
		char c = hex[i];

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

#endif
