#ifndef _ACTIVE_SESSION_HPP
#define _ACTIVE_SESSION_HPP

#include "../Common/CowBuffer.hpp"
#include "../Common/MyString.hpp"

#define SESSION_COMMAND_KEEP_ALIVE 1
#define SESSION_COMMAND_TEXT_MESSAGE 2
#define SESSION_COMMAND_DELIVER_MESSAGE 3
#define SESSION_COMMAND_LIST_USERS 4
#define SESSION_COMMAND_GET_MESSAGES 5

#define SESSION_RESPONSE_OK 200
#define SESSION_RESPONSE_ERROR 100
#define SESSION_RESPONSE_ERROR_INVALID_USER 101
#define SESSION_RESPONSE_ERROR_MESSAGE_TOO_SHORT 102
#define SESSION_RESPONSE_ERROR_CONNECTION_LOST 103
#define SESSION_RESPONSE_ERROR_USER_OFFLINE 104
#define SESSION_RESPONSE_ERROR_USER_IN_VOICE 105
#define SESSION_RESPONSE_ERROR_YOU_IN_VOICE 106

#define SESSION_COMMAND_VOICE_INIT 500
#define SESSION_COMMAND_VOICE_REQUEST 501
#define SESSION_COMMAND_VOICE_END 502
#define SESSION_COMMAND_VOICE_DATA 503
#define SESSION_RESPONSE_VOICE_RINGING 510
#define SESSION_RESPONSE_VOICE_ACCEPT 511
#define SESSION_RESPONSE_VOICE_DECLINE 512

namespace CommandKeepAlive
{
	struct Command
	{
		int64_t Timestamp;
	};

	bool ParseCommand(const CowBuffer<uint8_t> buffer, Command &result);
	CowBuffer<uint8_t> BuildCommand(const Command &data);
}

namespace CommandTextMessage
{
	struct Command
	{
		CowBuffer<uint8_t> Message;
	};

	struct Response
	{
		int32_t Status;
	};

	int32_t ParseCommand(const CowBuffer<uint8_t> buffer, Command &result);
	CowBuffer<uint8_t> BuildCommand(const Command &data);
	bool ParseResponse(const CowBuffer<uint8_t> buffer, Response &result);
	CowBuffer<uint8_t> BuildResponse(const Response &data);
}

namespace CommandDeliverMessage
{
	struct Command
	{
		CowBuffer<uint8_t> Message;
	};

	bool ParseCommand(const CowBuffer<uint8_t> buffer, Command &result);
	CowBuffer<uint8_t> BuildCommand(const Command &data);
}

namespace CommandListUsers
{
	struct Response
	{
		struct UserData
		{
			const uint8_t *Key;
			String Name;
		};

		CowBuffer<UserData> Data;
	};

	CowBuffer<uint8_t> BuildCommand();
	bool ParseResponse(const CowBuffer<uint8_t> buffer, Response &result);
	CowBuffer<uint8_t> BuildResponse(const Response &data);
}

namespace CommandGetMessages
{
	struct Command
	{
		int64_t Timestamp;
	};

	bool ParseCommand(const CowBuffer<uint8_t> buffer, Command &result);
	CowBuffer<uint8_t> BuildCommand(const Command &data);
}

namespace CommandVoiceInit
{
	struct Command
	{
		const uint8_t *Key;
		int64_t Timestamp;
	};

	struct Response
	{
		int32_t Status;
	};

	bool ParseCommand(const CowBuffer<uint8_t> buffer, Command &result);
	CowBuffer<uint8_t> BuildCommand(const Command &data);
	bool ParseResponse(const CowBuffer<uint8_t> buffer, Response &result);
	CowBuffer<uint8_t> BuildResponse(const Response &data);
}

namespace CommandVoiceRequest
{
	struct Command
	{
		const uint8_t *Key;
		int64_t Timestamp;
	};

	struct Response
	{
		int32_t Status;
	};

	bool ParseCommand(const CowBuffer<uint8_t> buffer, Command &result);
	CowBuffer<uint8_t> BuildCommand(const Command &data);
	bool ParseResponse(const CowBuffer<uint8_t> buffer, Response &result);
	CowBuffer<uint8_t> BuildResponse(const Response &data);
}

namespace CommandVoiceEnd
{
	CowBuffer<uint8_t> BuildCommand();
}

namespace CommandVoiceData
{
	struct Command
	{
		CowBuffer<uint8_t> VoiceData;
	};

	bool ParseCommand(const CowBuffer<uint8_t> buffer, Command &result);
	CowBuffer<uint8_t> BuildCommand(const Command &data);
}

#endif
