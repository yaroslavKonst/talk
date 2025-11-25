#include "Message.hpp"

#include <cstring>

bool Message::GetHeader(const CowBuffer<uint8_t> message, Header &result)
{
	if (message.Size() <= HeaderSize) {
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

CowBuffer<uint8_t> Message::BuildMessage(
	const CowBuffer<uint8_t> header,
	const CowBuffer<uint8_t> message)
{
	return header.Concat(message);
}
