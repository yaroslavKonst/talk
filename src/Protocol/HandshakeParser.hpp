#ifndef _HANDSHAKE_PARSER_HPP
#define _HANDSHAKE_PARSER_HPP

#include "../Common/CowBuffer.hpp"
#include "../Common/MyString.hpp"
#include "../Crypto/Crypto.hpp"

#define HANDSHAKE_RESPONSE_UNSUPPORTED_PROTOCOL_VERSION 1
#define HANDSHAKE_RESPONSE_UNSUPPORTED_ENCRYPTION_SCHEME 2

namespace Handshake
{
	enum
	{
		ChallengeSize = 64
	};
}

// Parser for Syn before encryption.
namespace HandshakeSyn
{
	struct Data
	{
		int32_t ProtocolVersion;
		int32_t EncryptionScheme;
		CowBuffer<uint8_t> Salt1;
		CowBuffer<uint8_t> OneTimeSalt;
		Crypto::X25519::PublicKeyContainer OneTimeKey;
		CowBuffer<uint8_t> EncryptedName; // | MAC | nonce | name |
	};

	bool Parse(const CowBuffer<uint8_t> buffer, Data &result);
	CowBuffer<uint8_t> Build(const Data &data);
}

// Parser for decrypted SynAck.
namespace HandshakeSynAck
{
	struct Data
	{
		int32_t ProtocolVersion;
		int32_t EncryptionScheme;
		CowBuffer<uint8_t> Challenge;
		Crypto::X25519::PublicKeyContainer ServerSessionPublicKey;
		CowBuffer<uint8_t> Salt2;
	};

	bool Parse(const CowBuffer<uint8_t> buffer, Data &result);
	CowBuffer<uint8_t> Build(const Data &data);
}

// Parser for decrypted Ack.
namespace HandshakeAck
{
	enum : uint64_t
	{
		Length =
			(uint64_t)Handshake::ChallengeSize +
			(uint64_t)Crypto::X25519::KEY_SIZE
	};

	struct Data
	{
		CowBuffer<uint8_t> Challenge;
		Crypto::X25519::PublicKeyContainer ClientSessionPublicKey;
	};

	bool Parse(const CowBuffer<uint8_t> buffer, Data &result);
	CowBuffer<uint8_t> Build(const Data &data);
}

#endif
