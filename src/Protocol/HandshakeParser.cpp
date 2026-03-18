#include "HandshakeParser.hpp"

#include <cstring>

#include "../Crypto/Crypto.hpp"

bool HandshakeSyn::Parse(const CowBuffer<uint8_t> buffer, Data &result)
{
	if (buffer.Size() != Length) {
		return false;
	}

	result.Key = buffer.Pointer();
	return true;
}

CowBuffer<uint8_t> HandshakeSyn::Build(const Data &data)
{
	CowBuffer<uint8_t> result(Length);
	memcpy(result.Pointer(), data.Key, KEY_SIZE);
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
