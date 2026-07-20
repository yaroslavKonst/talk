#ifndef _PARSER_HELPERS_HPP
#define _PARSER_HELPERS_HPP

#include "../Common/CowBuffer.hpp"
#include "../Common/MyString.hpp"

bool ParseString(
	const CowBuffer<uint8_t> buffer,
	uint64_t &offset,
	String &result,
	uint64_t lengthLimit = 0);

void BuildString(
	CowBuffer<uint8_t> &buffer,
	uint64_t &offset,
	const String &text);

uint64_t BuiltStringSize(const String text);

#endif
