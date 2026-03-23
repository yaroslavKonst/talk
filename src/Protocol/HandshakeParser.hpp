#ifndef _HANDSHAKE_PARSER_HPP
#define _HANDSHAKE_PARSER_HPP

#include "../Common/CowBuffer.hpp"
#include "../Common/MyString.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

namespace Handshake
{
	enum
	{
		ChallengeSize = 64,
		EncryptedChallengeSize = ChallengeSize + CRYPTO_HEADER_SIZE
	};
}

namespace HandshakeSyn
{
	struct Data
	{
		String Name;
	};

	bool Parse(const CowBuffer<uint8_t> buffer, Data &result);
	CowBuffer<uint8_t> Build(const Data &data);
}

namespace HandshakeSynAck
{
	enum
	{
		Length = sizeof(int64_t) + Handshake::EncryptedChallengeSize
	};

	struct Data
	{
		int64_t Timestamp;
		const uint8_t *Challenge;
	};

	bool Parse(const CowBuffer<uint8_t> buffer, Data &result);
	CowBuffer<uint8_t> Build(const Data &data);
}

namespace HandshakeAck
{
	enum
	{
		Length = Handshake::EncryptedChallengeSize
	};

	struct Data
	{
		const uint8_t *Challenge;
	};

	bool Parse(const CowBuffer<uint8_t> buffer, Data &result);
	CowBuffer<uint8_t> Build(const Data &data);
}

#endif
