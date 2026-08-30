#include "StreamParser.hpp"

#include "ParserHelpers.hpp"
#include "CommonParserConstants.hpp"
#include "../Message/Message.hpp"
#include "../Common/Endianness.hpp"

bool StreamHandshake::ParseInitRequest(
	const CowBuffer<uint8_t> buffer,
	InitRequest &data)
{
	uint64_t offset = 0;

	if (!ParseString(
		buffer,
		offset,
		data.Source,
		CommonParserConstants::FullNameSize))
	{
		return false;
	}

	if (!data.Source.Length()) {
		return false;
	}

	if (!Message::VerifyFullUserName(data.Source)) {
		return false;
	}

	if (buffer.Size() < offset + Crypto::X25519::KEY_SIZE) {
		return false;
	}

	data.SourceKey = buffer.Pointer(offset);
	offset += Crypto::X25519::KEY_SIZE;

	if (!ParseString(
		buffer,
		offset,
		data.Destination,
		CommonParserConstants::FullNameSize))
	{
		return false;
	}

	if (!data.Destination.Length()) {
		return false;
	}

	if (!Message::VerifyFullUserName(data.Destination)) {
		return false;
	}

	if (buffer.Size() < offset + Crypto::X25519::KEY_SIZE) {
		return false;
	}

	data.DestinationKey = buffer.Pointer(offset);
	offset += Crypto::X25519::KEY_SIZE;

	if (buffer.Size() < offset + SaltSize) {
		return false;
	}

	data.Salt = buffer.Slice(offset, SaltSize);
	offset += SaltSize;

	if (buffer.Size() !=
		offset + Crypto::X25519::KEY_SIZE + ChallengeSize +
		Crypto::X25519::CRYPTO_HEADER_SIZE)
	{
		return false;
	}

	data.ProtectedPart = buffer.Slice(
		offset,
		Crypto::X25519::KEY_SIZE + ChallengeSize +
		Crypto::X25519::CRYPTO_HEADER_SIZE);
	return true;
}

CowBuffer<uint8_t> StreamHandshake::BuildInitRequest(const InitRequest &data)
{
	uint32_t size =
		BuiltStringSize(data.Source) +
		Crypto::X25519::KEY_SIZE +
		BuiltStringSize(data.Destination) +
		Crypto::X25519::KEY_SIZE +
		data.Salt.Size() +
		data.ProtectedPart.Size();

	CowBuffer<uint8_t> buffer(size);
	uint64_t offset = 0;

	BuildString(buffer, offset, data.Source);
	memcpy(
		buffer.Pointer(offset),
		data.SourceKey.Key,
		Crypto::X25519::KEY_SIZE);
	offset += Crypto::X25519::KEY_SIZE;

	BuildString(buffer, offset, data.Destination);
	memcpy(
		buffer.Pointer(offset),
		data.DestinationKey.Key,
		Crypto::X25519::KEY_SIZE);
	offset += Crypto::X25519::KEY_SIZE;

	if (data.Salt.Size()) {
		memcpy(
			buffer.Pointer(offset),
			data.Salt.Pointer(),
			data.Salt.Size());
		offset += data.Salt.Size();
	}

	if (data.ProtectedPart.Size()) {
		memcpy(
			buffer.Pointer(offset),
			data.ProtectedPart.Pointer(),
			data.ProtectedPart.Size());
	}

	return buffer;
}

bool StreamHandshake::ParseProtectedInitRequest(
	const CowBuffer<uint8_t> buffer,
	ProtectedInitRequest &data)
{
	if (buffer.Size() != ChallengeSize + Crypto::X25519::KEY_SIZE) {
		return false;
	}

	data.EphemeralKey = buffer.Pointer();
	data.Challenge = buffer.Slice(Crypto::X25519::KEY_SIZE, ChallengeSize);
	return true;
}

CowBuffer<uint8_t> StreamHandshake::BuildProtectedInitRequest(
	const ProtectedInitRequest &data)
{
	CowBuffer<uint8_t> buffer(Crypto::X25519::KEY_SIZE);

	memcpy(
		buffer.Pointer(),
		data.EphemeralKey.Key,
		Crypto::X25519::KEY_SIZE);

	return buffer.Concat(data.Challenge);
}

bool StreamHandshake::ParseProtectedPeerResponse(
	const CowBuffer<uint8_t> buffer,
	ProtectedPeerResponse &data)
{
	uint32_t validSize =
		sizeof(int32_t) +
		SaltSize +
		Crypto::X25519::KEY_SIZE +
		ChallengeSize;

	if (buffer.Size() != validSize) {
		return false;
	}

	data.ResponseCode = SetProtoEndian(*buffer.SwitchType<int32_t>());
	uint64_t offset = sizeof(int32_t);

	data.Salt = buffer.Slice(offset, SaltSize);
	offset += SaltSize;

	data.EphemeralKey = buffer.Pointer(offset);
	offset += Crypto::X25519::KEY_SIZE;

	data.Challenge = buffer.Slice(offset, ChallengeSize);
	return true;
}

CowBuffer<uint8_t> StreamHandshake::BuildProtectedPeerResponse(
	const ProtectedPeerResponse &data)
{
	CowBuffer<uint8_t> buffer(
		sizeof(int32_t) +
		data.Salt.Size() +
		Crypto::X25519::KEY_SIZE +
		data.Challenge.Size());

	*buffer.SwitchType<int32_t>() = SetProtoEndian(data.ResponseCode);
	uint64_t offset = sizeof(int32_t);

	memcpy(
		buffer.Pointer(offset),
		data.Salt.Pointer(),
		data.Salt.Size());
	offset += data.Salt.Size();

	memcpy(
		buffer.Pointer(offset),
		data.EphemeralKey.Key,
		Crypto::X25519::KEY_SIZE);
	offset += Crypto::X25519::KEY_SIZE;

	memcpy(
		buffer.Pointer(offset),
		data.Challenge.Pointer(),
		data.Challenge.Size());

	return buffer;
}
