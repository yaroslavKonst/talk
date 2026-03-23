#include "HandshakeParser.hpp"

#include <cstring>

#include "../Crypto/Crypto.hpp"

bool HandshakeSyn::Parse(const CowBuffer<uint8_t> buffer, Data &result)
{
	int32_t nameLength;

	if (buffer.Size() < sizeof(nameLength)) {
		return false;
	}

	nameLength = *buffer.SwitchType<int32_t>();

	if (nameLength > 200 || nameLength <= 0) {
		return false;
	}

	if (buffer.Size() != sizeof(nameLength) + nameLength + 1) {
		return false;
	}

	if (buffer[sizeof(nameLength) + nameLength] != 0) {
		return false;
	}

	result.Name = buffer.SwitchType<char>(sizeof(nameLength));
	return true;
}

CowBuffer<uint8_t> HandshakeSyn::Build(const Data &data)
{
	CowBuffer<uint8_t> result(sizeof(int32_t) + data.Name.Length() + 1);
	*result.SwitchType<int32_t>() = data.Name.Length();
	memcpy(
		result.Pointer(sizeof(int32_t)),
		data.Name.CStr(),
		data.Name.Length() + 1);
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
