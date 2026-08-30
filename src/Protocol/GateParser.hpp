#ifndef _GATE_PARSER_HPP
#define _GATE_PARSER_HPP

#include "../Common/CowBuffer.hpp"
#include "../Crypto/Crypto.hpp"

#define GATE_HANDSHAKE_INIT_PROCEED 0
#define GATE_HANDSHAKE_INIT_REQUEST_RATE_LIMIT_REACHED 1

#define GATE_HANDSHAKE_VERIFICATION_SUCCESS 0
#define GATE_HANDSHAKE_VERIFICATION_FAILURE 1
#define GATE_HANDSHAKE_UNSUPPORTED_PROTOCOL_VERSION 2
#define GATE_HANDSHAKE_UNSUPPORTED_ENCRYPTION_SCHEME 3

namespace GateHandshake
{
	enum
	{
		SaltSize = 32
	};
}

namespace GateHandshakeStatus
{
	struct Data
	{
		int32_t Status;
	};

	bool ParseData(const CowBuffer<uint8_t> buffer, Data &result);
	CowBuffer<uint8_t> BuildData(const Data &data);
}

namespace GateHandshakeSyn
{
	struct Data
	{
		enum class Status
		{
			// Full message is received. ErrorCode field
			// must be ignored.
			Syn,

			// Error code is received. Other fields must be
			// ignored.
			ErrorCode
		};

		Status Stat;

		int32_t ErrorCode;

		int32_t ProtocolVersion;
		int32_t EncryptionScheme;
		String ServerName;
		Crypto::X25519::PublicKeyContainer Key;
		CowBuffer<uint8_t> Salt;
		CowBuffer<uint8_t> Signature; // Has zero length if not present.
	};

	bool ParseData(const CowBuffer<uint8_t> buffer, Data &result);
	CowBuffer<uint8_t> BuildData(const Data &data);
}

#define GATE_COMMAND_MESSAGE 1
#define GATE_COMMAND_MESSAGE_TEXT 1

#define GATE_MESSAGE_HEADER_ACCEPT 0
#define GATE_MESSAGE_HEADER_REJECT 1
#define GATE_MESSAGE_HEADER_REJECT_INVALID_DESTINATION_USER 2
#define GATE_MESSAGE_HEADER_REJECT_INVALID_DESTINATION_KEY 3
#define GATE_MESSAGE_HEADER_REJECT_INVALID_HEADER 4
#define GATE_MESSAGE_HEADER_REJECT_MESSAGE_TOO_BIG 5
#define GATE_MESSAGE_HEADER_REJECT_SENDER_BANNED 6
#define GATE_MESSAGE_HEADER_REJECT_SENDER_KEY_BANNED 7
#define GATE_MESSAGE_HEADER_REJECT_EXISTS 8

// Not from protocol specification, only for implementation internal usage.
#define GATE_MESSAGE_HEADER_REJECT_SILENTBLOCK 100
#define GATE_MESSAGE_HEADER_REJECT_CONNECTION_FAILURE 101

#define GATE_MESSAGE_BODY_ACCEPT 0
#define GATE_MESSAGE_BODY_REJECT 1
#define GATE_MESSAGE_BODY_REJECT_INVALID_SIZE 2

namespace GateCommandMessage
{
	struct Header
	{
		CowBuffer<uint8_t> MessageHeader;
	};

	struct Text
	{
		String Text;
	};

	struct VerificationCode
	{
		int32_t Code;
	};

	bool ParseHeader(const CowBuffer<uint8_t> buffer, Header &data);
	CowBuffer<uint8_t> BuildHeader(const Header &data);

	bool ParseText(const CowBuffer<uint8_t> buffer, Text &data);
	CowBuffer<uint8_t> BuildText(const Text &data);

	bool ParseCode(const CowBuffer<uint8_t> buffer, VerificationCode &data);
	CowBuffer<uint8_t> BuildCode(const VerificationCode &data);
}

#define GATE_COMMAND_STREAM_REQUEST 2

#define GATE_STREAM_FRAME 0
#define GATE_STREAM_END 1
#define GATE_STREAM_KEEP_ALIVE 2

namespace GateCommandStream
{
	struct InitRequest
	{
		CowBuffer<uint8_t> Request;
	};

	struct InitResponse
	{
		int32_t Code;
	};

	struct Frame
	{
		CowBuffer<uint8_t> Payload;
	};

	bool ParseInitRequest(
		const CowBuffer<uint8_t> buffer,
		InitRequest &data);
	CowBuffer<uint8_t> BuildInitRequest(const InitRequest &data);

	bool ParseInitResponse(
		const CowBuffer<uint8_t> buffer,
		InitResponse &data);
	CowBuffer<uint8_t> BuildInitResponse(const InitResponse &data);

	bool ParseFrame(const CowBuffer<uint8_t> buffer, Frame &data);
	CowBuffer<uint8_t> BuildFrame(const Frame &data);
}

#endif
