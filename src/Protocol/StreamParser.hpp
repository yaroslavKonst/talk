#ifndef _STREAM_PARSER_HPP
#define _STREAM_PARSER_HPP

#include "../Common/MyString.hpp"
#include "../Common/CowBuffer.hpp"
#include "../Crypto/Crypto.hpp"

#define STREAM_INIT_RESPONSE_WAITING_FOR_ANSWER 0
#define STREAM_INIT_RESPONSE_ERROR 1
#define STREAM_INIT_RESPONSE_YOU_ARE_IN_CALL 2
#define STREAM_INIT_RESPONSE_SERVER_OFFLINE 3
#define STREAM_INIT_RESPONSE_USER_OFFLINE 4
#define STREAM_INIT_RESPONSE_USER_BUSY 5
#define STREAM_INIT_RESPONSE_USER_NONEXISTENT 6
#define STREAM_INIT_RESPONSE_INVALID_DESTINATION_KEY 7
#define STREAM_INIT_RESPONSE_CALL_PROHIBITED 8
#define STREAM_INIT_RESPONSE_YOU_ARE_BANNED 9
#define STREAM_INIT_RESPONSE_YOUR_KEY_IS_BANNED 10
#define STREAM_INIT_RESPONSE_PARSING_FAILURE 11

#define STREAM_PEER_RESPONSE_ACCEPT 0
#define STREAM_PEER_RESPONSE_DECLINE 1

#define STREAM_COMMAND_LINE_INIT 0
#define STREAM_COMMAND_LINE_END 1
#define STREAM_COMMAND_LINE_DATA 2

#define STREAM_RESPONSE_LINE_ACCEPT 0
#define STREAM_RESPONSE_LINE_DECLINE 1

namespace StreamHandshake
{
	enum
	{
		SaltSize = 32,
		ChallengeSize = 64
	};

	struct InitRequest
	{
		String Source;
		Crypto::X25519::PublicKeyContainer SourceKey;
		String Destination;
		Crypto::X25519::PublicKeyContainer DestinationKey;
		CowBuffer<uint8_t> Salt;
		CowBuffer<uint8_t> ProtectedPart;
	};

	struct ProtectedInitRequest
	{
		Crypto::X25519::PublicKeyContainer EphemeralKey;
		CowBuffer<uint8_t> Challenge;
	};

	bool ParseInitRequest(
		const CowBuffer<uint8_t> buffer,
		InitRequest &data);
	CowBuffer<uint8_t> BuildInitRequest(const InitRequest &data);

	bool ParseProtectedInitRequest(
		const CowBuffer<uint8_t> buffer,
		ProtectedInitRequest &data);
	CowBuffer<uint8_t> BuildProtectedInitRequest(
		const ProtectedInitRequest &data);

	struct ProtectedPeerResponse
	{
		int32_t ResponseCode;
		CowBuffer<uint8_t> Salt;
		Crypto::X25519::PublicKeyContainer EphemeralKey;
		CowBuffer<uint8_t> Challenge;
	};

	bool ParseProtectedPeerResponse(
		const CowBuffer<uint8_t> buffer,
		ProtectedPeerResponse &data);
	CowBuffer<uint8_t> BuildProtectedPeerResponse(
		const ProtectedPeerResponse &data);
}

#endif
