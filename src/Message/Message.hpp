#ifndef _MESSAGE_HPP
#define _MESSAGE_HPP

#include "../Common/CowBuffer.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

namespace Message
{
	enum Constants
	{
		HeaderSize = KEY_SIZE * 2 + sizeof(int64_t) + sizeof(int32_t),
		SourceOffset = 0,
		DestinationOffset = KEY_SIZE,
		TimestampOffset = KEY_SIZE * 2,
		IndexOffset = KEY_SIZE * 2 + sizeof(int64_t),
	};

	struct Header
	{
		const uint8_t *Source;
		const uint8_t *Destination;
		int64_t Timestamp;
		int32_t Index;
	};

	struct MessageID
	{
		uint8_t Source[KEY_SIZE];
		uint8_t Destination[KEY_SIZE];

		int64_t Timestamp;
		int32_t Index;

		MessageID &operator=(const Header &header);

		bool operator==(const MessageID &id) const;
		bool operator<(const MessageID &id) const;
	};

	bool GetID(const CowBuffer<uint8_t> message, MessageID &result);

	bool GetHeader(const CowBuffer<uint8_t> message, Header &result);
	bool GetMessage(
		const CowBuffer<uint8_t> message,
		CowBuffer<uint8_t> &result);

	CowBuffer<uint8_t> BuildHeader(const Header &header);
	CowBuffer<uint8_t> BuildHeader(const MessageID &header);
	CowBuffer<uint8_t> BuildMessage(
		const CowBuffer<uint8_t> header,
		const CowBuffer<uint8_t> message);
};

#endif
