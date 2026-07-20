#include "HandshakeParser.hpp"

#include <cstring>

#include "ParserHelpers.hpp"
#include "../Crypto/Crypto.hpp"

static const uint64_t MaxNameLength = 500;

bool HandshakeSyn::Parse(const CowBuffer<uint8_t> buffer, Data &result)
{
	uint64_t offset = 0;

	if (buffer.Size() < offset + sizeof(int32_t)) {
		return false;
	}

	result.ProtocolVersion = *buffer.SwitchType<int32_t>(offset);
	offset += sizeof(int32_t);

	if (buffer.Size() < offset + sizeof(int32_t)) {
		return false;
	}

	result.EncryptionScheme = *buffer.SwitchType<int32_t>(offset);
	offset += sizeof(int32_t);

	if (!ParseString(buffer, offset, result.Name, MaxNameLength)) {
		return false;
	}

	if (!result.Name.Length()) {
		return false;
	}

	if (offset != buffer.Size()) {
		return false;
	}

	return true;
}

CowBuffer<uint8_t> HandshakeSyn::Build(const Data &data)
{
	uint64_t size =
		sizeof(int32_t) +
		sizeof(int32_t) +
		BuiltStringSize(data.Name);

	CowBuffer<uint8_t> result(size);
	uint64_t offset = 0;

	*result.SwitchType<int32_t>(offset) = data.ProtocolVersion;
	offset += sizeof(int32_t);

	*result.SwitchType<int32_t>(offset) = data.EncryptionScheme;
	offset += sizeof(int32_t);

	BuildString(result, offset, data.Name);

	return result;
}

bool HandshakeSynAck::Parse(const CowBuffer<uint8_t> buffer, Data &result)
{
	if (buffer.Size() != Length) {
		return false;
	}

	result.Timestamp = *buffer.SwitchType<int64_t>();
	result.Challenge = buffer.Pointer(sizeof(int64_t));

	return true;
}

CowBuffer<uint8_t> HandshakeSynAck::Build(const Data &data)
{
	CowBuffer<uint8_t> result(Length);

	*result.SwitchType<int64_t>() = data.Timestamp;
	memcpy(
		result.Pointer(sizeof(int64_t)),
		data.Challenge,
		Handshake::EncryptedChallengeSize);

	return result;
}

bool HandshakeAck::Parse(const CowBuffer<uint8_t> buffer, Data &result)
{
	if (buffer.Size() != Length) {
		return false;
	}

	result.Challenge = buffer.Pointer();

	return true;
}

CowBuffer<uint8_t> HandshakeAck::Build(const Data &data)
{
	CowBuffer<uint8_t> result(Length);

	memcpy(
		result.Pointer(),
		data.Challenge,
		Handshake::EncryptedChallengeSize);

	return result;
}
