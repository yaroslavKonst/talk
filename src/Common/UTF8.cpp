#include "UTF8.hpp"

CowBuffer<uint32_t> UTF8::Decode(String str)
{
	CowBuffer<uint32_t> result(StrLen(str.CStr()));

	if (!result.Size()) {
		return result;
	}

	Decoder decoder;
	int index = 0;

	for (int i = 0; i < str.Length(); i++) {
		bool success = decoder.AddByte((uint8_t)str.CStr()[i]);

		if (!success) {
			return CowBuffer<uint32_t>();
		}

		if (decoder.HasChar()) {
			result[index] = decoder.GetChar();
			++index;
			decoder.Reset();
		}
	}

	return result;
}

String UTF8::Encode(CowBuffer<uint32_t> str)
{
	String result;

	for (unsigned int i = 0; i < str.Size(); i++) {
		result += Encode(str[i]);
	}

	return result;
}

String UTF8::Encode(uint32_t c)
{
	int digitCount = 0;
	uint32_t tmp = c;

	while (tmp) {
		++digitCount;
		tmp >>= 1;
	}

	if (digitCount < 8) {
		String result;
		result += c;
		return result;
	}

	int byteCount;

	if (digitCount < 12) {
		byteCount = 2;
	} else if (digitCount < 17) {
		byteCount = 3;
	} else if (digitCount < 22) {
		byteCount = 4;
	} else if (digitCount < 27) {
		byteCount = 5;
	} else if (digitCount < 32) {
		byteCount = 6;
	} else {
		byteCount = 7;
	}

	CowBuffer<char> result(byteCount + 1);

	for (int i = byteCount - 1; i > 0; i--) {
		result[i] = (c & 0x3f) | 0x80;
		c >>= 6;
	}

	char prefix = 0;

	for (int i = 0; i < byteCount; i++) {
		prefix |= 1 << (7 - i);
	}

	result[0] = prefix | c;
	result[byteCount] = 0;

	return result.Pointer();
}

UTF8::Decoder::Decoder()
{
	Reset();
}

void UTF8::Decoder::Reset()
{
	_expectedBytes = 0;
	_value = 0;
}

bool UTF8::Decoder::HasChar()
{
	return !_expectedBytes;
}

uint32_t UTF8::Decoder::GetChar()
{
	return _value;
}

bool UTF8::Decoder::AddByte(int c)
{
	if (c > 0xff || c < 0) {
		return false;
	}

	if (!_expectedBytes) {
		_expectedBytes = 0;

		if (IsTrailing(c)) {
			return false;
		}

		unsigned char tmp = c;
		uint32_t leadingOnes = 0;

		while (tmp & 0x80) {
			tmp <<= 1;
			++leadingOnes;
		}

		_value = tmp >> leadingOnes;

		_expectedBytes = leadingOnes ? leadingOnes - 1 : 0;
	} else {
		if (!IsTrailing(c)) {
			return false;
		}

		_value <<= 6;
		_value |= c & 0x3f;

		--_expectedBytes;
	}

	return true;
}
