#include "Handshake.hpp"

#include <cstring>

#include "../Crypto/Crypto.hpp"

bool Handshake1::Parse(const CowBuffer<uint8_t> buffer, Data &result)
{
	unsigned int validSize = KEY_SIZE + sizeof(result.Timestamp) +
		SIGNATURE_SIZE;

	if (buffer.Size() != validSize) {
		return false;
	}

	result.Key = buffer.Pointer();
	result.Timestamp = *buffer.SwitchType<int64_t>(KEY_SIZE);
	result.Signature = buffer.Slice(
		KEY_SIZE + sizeof(result.Timestamp),
		SIGNATURE_SIZE);
	return true;
}

CowBuffer<uint8_t> Handshake1::Build(
	const Data &data,
	const uint8_t *signatureKey)
{
	CowBuffer<uint8_t> result(
		KEY_SIZE + sizeof(data.Timestamp));

	memcpy(result.Pointer(), data.Key, KEY_SIZE);
	*result.SwitchType<int64_t>(KEY_SIZE) = data.Timestamp;

	CowBuffer<uint8_t> signature(SIGNATURE_SIZE);
	Sign(result, signatureKey, signature.Pointer());

	return result.Concat(signature);
}

bool Handshake2::Parse(const CowBuffer<uint8_t> buffer, Data &result)
{
	unsigned int validSize = KEY_SIZE + sizeof(result.Timestamp);

	if (buffer.Size() != validSize) {
		return false;
	}

	result.Key = buffer.Pointer();
	result.Timestamp = *buffer.SwitchType<int64_t>(KEY_SIZE);
	return true;
}

CowBuffer<uint8_t> Handshake2::Build(const Data &data)
{
	CowBuffer<uint8_t> result(KEY_SIZE + sizeof(data.Timestamp));

	memcpy(result.Pointer(), data.Key, KEY_SIZE);
	*result.SwitchType<int64_t>(KEY_SIZE) = data.Timestamp;

	return result;
}

bool Handshake3::Parse(const CowBuffer<uint8_t> buffer, Data &result)
{
	unsigned int validSize = sizeof(result.Timestamp);

	if (buffer.Size() != validSize) {
		return false;
	}

	result.Timestamp = *buffer.SwitchType<int64_t>();
	return true;
}

CowBuffer<uint8_t> Handshake3::Build(const Data &data)
{
	CowBuffer<uint8_t> result(sizeof(data.Timestamp));
	*result.SwitchType<int64_t>() = data.Timestamp;
	return result;
}
