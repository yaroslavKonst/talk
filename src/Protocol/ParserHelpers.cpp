#include "ParserHelpers.hpp"

#include "../Common/Endianness.hpp"

bool ParseString(
	const CowBuffer<uint8_t> buffer,
	uint64_t &offset,
	String &result,
	uint64_t lengthLimit)
{
	if (buffer.Size() < offset + sizeof(int32_t)) {
		return false;
	}

	uint32_t stringLength =
		SetProtoEndian(*buffer.SwitchType<uint32_t>(offset));
	offset += sizeof(uint32_t);

	if (lengthLimit && stringLength > lengthLimit) {
		return false;
	}

	if (buffer.Size() < offset + stringLength) {
		return false;
	}

	result = String(buffer.SwitchType<char>(offset), stringLength);
	offset += stringLength;

	return true;
}

void BuildString(
	CowBuffer<uint8_t> &buffer,
	uint64_t &offset,
	const String &text)
{
	*buffer.SwitchType<uint32_t>(offset) =
		SetProtoEndian<uint32_t>(text.Length());
	offset += sizeof(uint32_t);

	memcpy(buffer.Pointer(offset), text.CStr(), text.Length());
	offset += text.Length();
}

uint64_t BuiltStringSize(const String text)
{
	return sizeof(uint32_t) + text.Length();
}
