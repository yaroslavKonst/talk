#include "GateParser.hpp"

#include <cstring>

#include "ParserHelpers.hpp"
#include "../Common/Endianness.hpp"

using namespace Crypto::X25519;

static const uint64_t MaxNameLength = 500;
static const uint64_t SaltSize = 32;

bool GateHandshakeStatus::ParseData(
	const CowBuffer<uint8_t> buffer,
	Data &result)
{
	if (buffer.Size() != sizeof(int32_t)) {
		return false;
	}

	result.Status = SetProtoEndian(*buffer.SwitchType<int32_t>(0));
	return true;
}

CowBuffer<uint8_t> GateHandshakeStatus::BuildData(const Data &data)
{
	CowBuffer<uint8_t> result(sizeof(int32_t));
	*result.SwitchType<int32_t>(0) = SetProtoEndian(data.Status);
	return result;
}

bool GateHandshakeSyn::ParseData(const CowBuffer<uint8_t> buffer, Data &result)
{
	// Error code variant carries only a 32 bit error code.
	if (buffer.Size() == sizeof(int32_t)) {
		result.Stat = Data::Status::ErrorCode;
		result.ErrorCode =
			SetProtoEndian(*buffer.SwitchType<int32_t>(0));

		return true;
	}

	result.Stat = Data::Status::Syn;

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

	if (!ParseString(buffer, offset, result.ServerName, MaxNameLength)) {
		return false;
	}

	if (!result.ServerName.Length()) {
		return false;
	}

	if (buffer.Size() < offset + (uint64_t)KEY_SIZE) {
		return false;
	}

	result.Key = buffer.Pointer(offset);
	offset += KEY_SIZE;

	if (buffer.Size() < offset + SaltSize) {
		return false;
	}

	result.Salt = buffer.Slice(offset, SaltSize);
	offset += SaltSize;

	// The optional signature covers the rest of the message.
	uint64_t remaining = buffer.Size() - offset;

	if (remaining == 0) {
		result.Signature = CowBuffer<uint8_t>();
	} else if (remaining == SIGNATURE_SIZE) {
		result.Signature = buffer.Slice(offset, SIGNATURE_SIZE);
	} else {
		return false;
	}

	return true;
}

CowBuffer<uint8_t> GateHandshakeSyn::BuildData(const Data &data)
{
	if (data.Stat == Data::Status::ErrorCode) {
		CowBuffer<uint8_t> result(sizeof(int32_t));
		*result.SwitchType<int32_t>(0) = SetProtoEndian(data.ErrorCode);
		return result;
	}

	uint64_t size =
		sizeof(int32_t) +
		sizeof(int32_t) +
		BuiltStringSize(data.ServerName) +
		(uint64_t)KEY_SIZE +
		data.Salt.Size() +
		data.Signature.Size();

	CowBuffer<uint8_t> result(size);
	uint64_t offset = 0;

	*result.SwitchType<int32_t>(offset) =
		SetProtoEndian(data.ProtocolVersion);
	offset += sizeof(int32_t);

	*result.SwitchType<int32_t>(offset) =
		SetProtoEndian(data.EncryptionScheme);
	offset += sizeof(int32_t);

	BuildString(result, offset, data.ServerName);

	memcpy(result.Pointer(offset), data.Key.Key, KEY_SIZE);
	offset += KEY_SIZE;

	memcpy(
		result.Pointer(offset),
		data.Salt.Pointer(),
		data.Salt.Size());
	offset += data.Salt.Size();

	if (data.Signature.Size()) {
		memcpy(
			result.Pointer(offset),
			data.Signature.Pointer(),
			data.Signature.Size());
	}

	return result;
}
