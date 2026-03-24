#ifndef _MESSAGE_HPP
#define _MESSAGE_HPP

#include "../Common/CowBuffer.hpp"
#include "../Common/MyString.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

namespace Message
{
	enum class Type
	{
		PointToPoint = 0,
		Group = 1
	};

	struct HeaderPointToPoint
	{
		uint8_t Type;

		String Source;
		uint8_t SourceKey[KEY_SIZE];

		String Destination
		uint8_t DestinationKey[KEY_SIZE];

		int64_t Timestamp;
		int32_t Index;

		uint64_t HeaderSize;
	};

	bool ParseHeader(
		const CowBuffer<uint8_t> message,
		HeaderPointToPoint &result);
	CowBuffer<uint8_t> BuildHeader(const HeaderPointToPoint &header);
};

#endif
