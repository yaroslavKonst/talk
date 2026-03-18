#include "Message.hpp"

#include <cstring>

#include "../ThirdParty/monocypher.h"

bool Message::MessageID::operator==(const MessageID &id) const
{
	if (Timestamp != id.Timestamp || Index != id.Index) {
		return false;
	}

	if (crypto_verify32(Source, id.Source)) {
		return false;
	}

	return !crypto_verify32(Destination, id.Destination);
}

bool Message::MessageID::operator<(const MessageID &id) const
{
	if (Timestamp != id.Timestamp) {
		return Timestamp < id.Timestamp;
	}

	if (Index != id.Index) {
		return Index < id.Index;
	}

	for (int i = 0; i < KEY_SIZE; i++) {
		if (Source[i] != id.Source[i]) {
			return Source[i] < id.Source[i];
		}
	}

	for (int i = 0; i < KEY_SIZE; i++) {
		if (Destination[i] != id.Destination[i]) {
			return Destination[i] < id.Destination[i];
		}
	}

	return false;
}

bool Message::GetID(
	const CowBuffer<uint8_t> message,
	Message::MessageID &result)
{
	if (message.Size() < HeaderSize) {
		return false;
	}

	result.Timestamp = *message.SwitchType<int64_t>(TimestampOffset);
	result.Index = *message.SwitchType<int32_t>(IndexOffset);

	memcpy(result.Source, message.Pointer(SourceOffset), KEY_SIZE);
	memcpy(
		result.Destination,
		message.Pointer(DestinationOffset),
		KEY_SIZE);

	return true;
}

bool Message::GetHeader(const CowBuffer<uint8_t> message, Header &result)
{
	if (message.Size() < HeaderSize) {
		return false;
	}

	result.Source = message.Pointer(SourceOffset);
	result.Destination = message.Pointer(DestinationOffset);
	result.Timestamp = *message.SwitchType<int64_t>(TimestampOffset);
	result.Index = *message.SwitchType<int32_t>(IndexOffset);
	return true;
}

bool Message::GetMessage(
	const CowBuffer<uint8_t> message,
	CowBuffer<uint8_t> &result)
{
	if (message.Size() <= HeaderSize) {
		return false;
	}

	result = message.Slice(HeaderSize, message.Size() - HeaderSize);
	return true;
}

CowBuffer<uint8_t> Message::BuildHeader(const Header &header)
{
	CowBuffer<uint8_t> result(HeaderSize);

	memcpy(result.Pointer(SourceOffset), header.Source, KEY_SIZE);
	memcpy(result.Pointer(DestinationOffset), header.Destination, KEY_SIZE);
	*result.SwitchType<int64_t>(TimestampOffset) = header.Timestamp;
	*result.SwitchType<int32_t>(IndexOffset) = header.Index;
	return result;
}

CowBuffer<uint8_t> Message::BuildHeader(const MessageID &header)
{
	CowBuffer<uint8_t> result(HeaderSize);

	memcpy(result.Pointer(SourceOffset), header.Source, KEY_SIZE);
	memcpy(result.Pointer(DestinationOffset), header.Destination, KEY_SIZE);
	*result.SwitchType<int64_t>(TimestampOffset) = header.Timestamp;
	*result.SwitchType<int32_t>(IndexOffset) = header.Index;
	return result;
}

CowBuffer<uint8_t> Message::BuildMessage(
	const CowBuffer<uint8_t> header,
	const CowBuffer<uint8_t> message)
{
	return header.Concat(message);
}
