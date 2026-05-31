#ifndef _MESSAGE_HPP
#define _MESSAGE_HPP

#include "../Common/CowBuffer.hpp"
#include "../Common/MyString.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

namespace Message
{
	enum class Type
	{
		Invalid = 0,
		PointToPoint = 1,
		Group = 2
	};

	Type GetMessageType(const CowBuffer<uint8_t> message);

	struct HeaderPointToPoint
	{
		String Source;
		uint8_t SourceKey[KEY_SIZE];

		String Destination;
		uint8_t DestinationKey[KEY_SIZE];

		int64_t Timestamp;
		int32_t Index;

		uint64_t HeaderSize;
		uint64_t MessageSize;

		bool operator<(const HeaderPointToPoint &header) const;
		bool operator==(const HeaderPointToPoint &header) const;
	};

	bool ParseHeader(
		const CowBuffer<uint8_t> message,
		HeaderPointToPoint &header);
	CowBuffer<uint8_t> BuildHeader(const HeaderPointToPoint &header);
	void WriteHeaderSize(HeaderPointToPoint &header);

	struct HeaderGroup
	{
		String Source;
		uint8_t SourceKey[KEY_SIZE];

		String GroupName;
		CowBuffer<CowBuffer<uint8_t>> GroupKeys;

		int64_t Timestamp;
		int32_t Index;

		uint64_t HeaderSize;
		uint64_t MessageSize;

		bool operator<(const HeaderGroup &header) const;
		bool operator==(const HeaderGroup &header) const;
	};

	bool ParseHeader(
		const CowBuffer<uint8_t> message,
		HeaderGroup &result);
	CowBuffer<uint8_t> BuildHeader(const HeaderGroup &header);
	void WriteHeaderSize(HeaderGroup &header);

	struct Contents
	{
		String Text;
		CowBuffer<uint8_t> Attachment;
	};

	bool ParseContents(
		const CowBuffer<uint8_t> message,
		Contents &contents);
	CowBuffer<uint8_t> BuildContents(const Contents &contents);

	enum class Attribute : int32_t
	{
		// Direction.
		Inbound = 0x1,

		// Inbound attributes.
		Unread = 0x2,

		// Outbound attributes.
		InProgress = 0x2,
		ConnectionFailure = 0x4,
		NonExistentAddress = 0x8,
		MessageTooBig = 0x10
	};
};

#endif
