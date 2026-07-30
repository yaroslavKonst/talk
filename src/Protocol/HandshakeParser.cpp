#include "HandshakeParser.hpp"

#include <cstring>

#include "../Crypto/Crypto.hpp"
#include "../Common/Endianness.hpp"

using namespace Crypto::X25519;

static const uint64_t MaxNameLength = 500;
static const uint64_t SaltSize = 32;

static const uint64_t SynAckLength =
	sizeof(int32_t) +
	sizeof(int32_t) +
	(uint64_t)Handshake::ChallengeSize +
	(uint64_t)KEY_SIZE +
	SaltSize;

bool HandshakeSyn::Parse(const CowBuffer<uint8_t> buffer, Data &result)
{
	uint64_t offset = 0;

	if (buffer.Size() < offset + sizeof(int32_t)) {
		return false;
	}

	result.ProtocolVersion =
		SetProtoEndian(*buffer.SwitchType<int32_t>(offset));
	offset += sizeof(int32_t);

	if (buffer.Size() < offset + sizeof(int32_t)) {
		return false;
	}

	result.EncryptionScheme =
		SetProtoEndian(*buffer.SwitchType<int32_t>(offset));
	offset += sizeof(int32_t);

	if (buffer.Size() < offset + SaltSize) {
		return false;
	}

	result.Salt1 = buffer.Slice(offset, SaltSize);
	offset += SaltSize;

	if (buffer.Size() < offset + SaltSize) {
		return false;
	}

	result.OneTimeSalt = buffer.Slice(offset, SaltSize);
	offset += SaltSize;

	if (buffer.Size() < offset + (uint64_t)KEY_SIZE) {
		return false;
	}

	result.OneTimeKey = buffer.Pointer(offset);
	offset += KEY_SIZE;

	uint64_t minSize = (uint64_t)MAC_SIZE + (uint64_t)NONCE_SIZE + 1;
	uint64_t maxSize =
		(uint64_t)MAC_SIZE + (uint64_t)NONCE_SIZE + MaxNameLength;

	uint64_t encryptedNameSize = buffer.Size() - offset;

	if (encryptedNameSize < minSize || encryptedNameSize > maxSize) {
		return false;
	}

	result.EncryptedName = buffer.Slice(offset, encryptedNameSize);

	return true;
}

CowBuffer<uint8_t> HandshakeSyn::Build(const Data &data)
{
	uint64_t size =
		sizeof(int32_t) +
		sizeof(int32_t) +
		data.Salt1.Size() +
		data.OneTimeSalt.Size() +
		(uint64_t)KEY_SIZE +
		data.EncryptedName.Size();

	CowBuffer<uint8_t> result(size);
	uint64_t offset = 0;

	*result.SwitchType<int32_t>(offset) =
		SetProtoEndian(data.ProtocolVersion);
	offset += sizeof(int32_t);

	*result.SwitchType<int32_t>(offset) =
		SetProtoEndian(data.EncryptionScheme);
	offset += sizeof(int32_t);

	memcpy(
		result.Pointer(offset),
		data.Salt1.Pointer(),
		data.Salt1.Size());
	offset += data.Salt1.Size();

	memcpy(
		result.Pointer(offset),
		data.OneTimeSalt.Pointer(),
		data.OneTimeSalt.Size());
	offset += data.OneTimeSalt.Size();

	memcpy(result.Pointer(offset), data.OneTimeKey.Key, KEY_SIZE);
	offset += KEY_SIZE;

	if (data.EncryptedName.Size()) {
		memcpy(
			result.Pointer(offset),
			data.EncryptedName.Pointer(),
			data.EncryptedName.Size());
	}

	return result;
}

bool HandshakeSynAck::Parse(const CowBuffer<uint8_t> buffer, Data &result)
{
	if (buffer.Size() != SynAckLength) {
		return false;
	}

	uint64_t offset = 0;

	result.ProtocolVersion =
		SetProtoEndian(*buffer.SwitchType<int32_t>(offset));
	offset += sizeof(int32_t);

	result.EncryptionScheme =
		SetProtoEndian(*buffer.SwitchType<int32_t>(offset));
	offset += sizeof(int32_t);

	result.Challenge = buffer.Slice(offset, Handshake::ChallengeSize);
	offset += Handshake::ChallengeSize;

	result.ServerSessionPublicKey = buffer.Pointer(offset);
	offset += KEY_SIZE;

	result.Salt2 = buffer.Slice(offset, SaltSize);

	return true;
}

CowBuffer<uint8_t> HandshakeSynAck::Build(const Data &data)
{
	CowBuffer<uint8_t> result(SynAckLength);
	uint64_t offset = 0;

	*result.SwitchType<int32_t>(offset) =
		SetProtoEndian(data.ProtocolVersion);
	offset += sizeof(int32_t);

	*result.SwitchType<int32_t>(offset) =
		SetProtoEndian(data.EncryptionScheme);
	offset += sizeof(int32_t);

	memcpy(
		result.Pointer(offset),
		data.Challenge.Pointer(),
		Handshake::ChallengeSize);
	offset += Handshake::ChallengeSize;

	memcpy(
		result.Pointer(offset),
		data.ServerSessionPublicKey.Key,
		KEY_SIZE);
	offset += KEY_SIZE;

	memcpy(result.Pointer(offset), data.Salt2.Pointer(), SaltSize);

	return result;
}

bool HandshakeAck::Parse(const CowBuffer<uint8_t> buffer, Data &result)
{
	if (buffer.Size() != Length) {
		return false;
	}

	uint64_t offset = 0;

	result.Challenge = buffer.Slice(offset, Handshake::ChallengeSize);
	offset += Handshake::ChallengeSize;

	result.ClientSessionPublicKey = buffer.Pointer(offset);

	return true;
}

CowBuffer<uint8_t> HandshakeAck::Build(const Data &data)
{
	CowBuffer<uint8_t> result(Length);
	uint64_t offset = 0;

	memcpy(
		result.Pointer(offset),
		data.Challenge.Pointer(),
		Handshake::ChallengeSize);
	offset += Handshake::ChallengeSize;

	memcpy(
		result.Pointer(offset),
		data.ClientSessionPublicKey.Key,
		KEY_SIZE);

	return result;
}
