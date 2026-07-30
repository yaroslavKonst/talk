#ifndef _OBJECT_TYPE_HPP
#define _OBJECT_TYPE_HPP

#include "../Message/Message.hpp"
#include "../Common/CowBuffer.hpp"
#include "../Common/ObjectStorage.hpp"
#include "../Crypto/Crypto.hpp"

#define HEAD_REF "Head"
#define ROOT_REF "Root"

enum class ObjectType
{
	NewContact = 0,
	UpdateContactKey = 1,
	BlockContact = 2,
	Message = 3,
	UpdateMessage = 4
};

namespace NewContactObject
{
	struct Data
	{
		String ContactName;
	};

	bool ParseData(const CowBuffer<uint8_t> object, Data &data);
	CowBuffer<uint8_t> BuildData(const Data &data);
}

namespace UpdateContactKeyObject
{
	struct Data
	{
		String ContactName;
		Crypto::X25519::PublicKeyContainer Key;
		uint8_t Validated;
		uint8_t Blocked;
		uint8_t SetAsDefault;
	};

	bool ParseData(const CowBuffer<uint8_t> object, Data &data);
	CowBuffer<uint8_t> BuildData(const Data &data);
}

namespace BlockContactObject
{
	struct Data
	{
		String ContactName;
		uint8_t BlockStatus;
	};

	bool ParseData(const CowBuffer<uint8_t> object, Data &data);
	CowBuffer<uint8_t> BuildData(const Data &data);
}

namespace MessageObject
{
	struct Data
	{
		String PeerName;
		ObjectStorage::ID HeaderHash;
	};

	bool ParseData(const CowBuffer<uint8_t> object, Data &data);
	CowBuffer<uint8_t> BuildData(const Data &data);
}

namespace UpdateMessageObject
{
	struct Data
	{
		String PeerName;
		ObjectStorage::ID HeaderHash;
		Message::Attribute Attr;
		bool Value;
	};

	bool ParseData(const CowBuffer<uint8_t> object, Data &data);
	CowBuffer<uint8_t> BuildData(const Data &data);
}

#endif
