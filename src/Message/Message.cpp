#include "Message.hpp"

#include <cstring>

#include "../ThirdParty/monocypher.h"

Message::Type Message::GetMessageType(const CowBuffer<uint8_t> message)
{
	if (message.Size() < 1) {
		return Type::Invalid;
	}

	switch (message[0]) {
	case Type::PointToPoint:
		return Type::PointToPoint;
	case Type::Group:
		return Type::Group;
	default:
		return type::Invalid;
	}
}

bool Message::ParseHeader(
	const CowBuffer<uint8_t> message,
	HeaderPointToPoint &header)
{
	if (message.Size() < 1) {
		return false;
	}

	header.Type = message[0];

	if (result.Type != (uint8_t)Type::PointToPoint) {
		return false;
	}

	uint64_t offset = 1;

	int32_t sourceSize;

	{
		const CowBuffer<uint8_t> slice = message.Slice(
			offset,
			message.Size() - offset);

		if (slice.Size() < sizeof(sourceSize)) {
			return false;
		}

		sourceSize = *slice.SwitchType<int32_t>();
		offset += sizeof(sourceSize);
	}

	if (sourceSize > 500) {
		return false;
	}

	{
		const CowBuffer<uint8_t> slice = message.Slice(
			offset,
			message.Size() - offset);

		if (slice.Size() < sourceSize) {
			return false;
		}

		header.Source = String(slice.SwitchType<char>(), sourceSize);
		offset += sourceSize;
	}

	{
		const CowBuffer<uint8_t> slice = message.Slice(
			offset,
			message.Size() - offset);

		if (slice.Size() < KEY_SIZE) {
			return false;
		}

		memcpy(header.SourceKey, slice.Pointer(), KEY_SIZE);
		offset += KEY_SIZE;
	}

	int32_t destinationSize;

	{
		const CowBuffer<uint8_t> slice = message.Slice(
			offset,
			message.Size() - offset);

		if (slice.Size() < sizeof(destinationSize)) {
			return false;
		}

		destinationSize = *slice.SwitchType<int32_t>();
		offset += sizeof(destinationSize);
	}

	if (destinationSize > 500) {
		return false;
	}

	{
		const CowBuffer<uint8_t> slice = message.Slice(
			offset,
			message.Size() - offset);

		if (slice.Size() < destinationSize) {
			return false;
		}

		header.Destination = String(
			slice.SwitchType<char>(),
			destinationSize);
		offset += destinationSize;
	}

	{
		const CowBuffer<uint8_t> slice = message.Slice(
			offset,
			message.Size() - offset);

		if (slice.Size() < KEY_SIZE) {
			return false;
		}

		memcpy(header.DestinationKey, slice.Pointer(), KEY_SIZE);
		offset += KEY_SIZE;
	}

	{
		const CowBuffer<uint8_t> slice = message.Slice(
			offset,
			message.Size() - offset);

		if (slice.Size() < sizeof(header.Timestamp)) {
			return false;
		}

		header.Timestamp = slice.SwitchType<int64_t>();
		offset += sizeof(header.Timestamp);
	}

	{
		const CowBuffer<uint8_t> slice = message.Slice(
			offset,
			message.Size() - offset);

		if (slice.Size() < sizeof(header.Index)) {
			return false;
		}

		header.Index = slice.SwitchType<int32_t>();
		offset += sizeof(header.Index);
	}

	{
		const CowBuffer<uint8_t> slice = message.Slice(
			offset,
			message.Size() - offset);

		if (slice.Size() < sizeof(header.MessageSize)) {
			return false;
		}

		header.MessageSize = *slice.SwitchType<uint64_t>();

		if (header.MessageSize < CRYPTO_HEADER_SIZE) {
			return false;
		}

		offset += sizeof(header.MessageSize);
	}

	header.HeaderSize = offset;

	return true;
}

CowBuffer<uint8_t> Message::BuildHeader(const HeaderPointToPoint &header)
{
	CowBuffer<uint8_t> buffer(header.HeaderSize);

	buffer[0] = (uint8_t)Type::PointToPoint;

	uint64_t offset = 1;

	*buffer.SwitchType<int32_t>(offset) = header.Source.Length();
	offset += sizeof(int32_t);

	memcpy(
		buffer.Pointer(offset),
		header.Source.CStr(),
		header.Source.Length() + 1);
	offset += header.Source.Length() + 1;

	memcpy(buffer.Pointer(offset), header.SourceKey, KEY_SIZE);
	offset += KEY_SIZE;

	*buffer.SwitchType<int32_t>(offset) = header.Destination.Length();
	offset += sizeof(int32_t);

	memcpy(
		buffer.Pointer(offset),
		header.Destination.CStr(),
		header.Destination.Length() + 1);
	offset += header.Destination.Length() + 1;

	memcpy(buffer.Pointer(offset), header.DestinationKey, KEY_SIZE);
	offset += KEY_SIZE;

	*buffer.SwitchType<int64_t>(offset) = header.Timestamp;
	offset += sizeof(header.Timestamp);

	*buffer.SwitchType<int32_t>(offset) = header.Index;
	offset += sizeof(header.Index);

	*buffer.SwitchType<uint64_t>(offset) = header.MessageSize;

	return buffer;
}
