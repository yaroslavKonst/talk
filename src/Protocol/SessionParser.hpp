#ifndef _ACTIVE_SESSION_HPP
#define _ACTIVE_SESSION_HPP

#include "../Message/Message.hpp"
#include "../Common/CowBuffer.hpp"
#include "../Common/MyString.hpp"
#include "../Common/ObjectStorage.hpp"

#define SESSION_COMMAND_KEEP_ALIVE 1
#define SESSION_COMMAND_GET_HOST_NAME 2
#define SESSION_COMMAND_REQUEST_ID 3
#define SESSION_COMMAND_UPDATE_ID 4
#define SESSION_COMMAND_ADD_CONTACT 5
#define SESSION_COMMAND_UPDATE_CONTACT_KEY 6
#define SESSION_COMMAND_BLOCK_CONTACT 7
#define SESSION_COMMAND_LIST_CONTACTS 8
#define SESSION_COMMAND_SEND_MESSAGE 9
#define SESSION_COMMAND_UPDATE_MESSAGE 10
#define SESSION_COMMAND_DELETE_MESSAGE 11

#define SESSION_RESPONSE_OK 200
#define SESSION_RESPONSE_ERROR 100
#define SESSION_RESPONSE_ERROR_INVALID_USER 101
#define SESSION_RESPONSE_ERROR_MESSAGE_TOO_SHORT 102
#define SESSION_RESPONSE_ERROR_CONNECTION_LOST 103
#define SESSION_RESPONSE_ERROR_USER_OFFLINE 104
#define SESSION_RESPONSE_ERROR_USER_BUSY 105
#define SESSION_RESPONSE_ERROR_YOU_BUSY 106

#define SESSION_COMMAND_STREAM_INIT 500
#define SESSION_COMMAND_STREAM_REQUEST 501
#define SESSION_COMMAND_STREAM_END 502
#define SESSION_COMMAND_STREAM_DATA 503
#define SESSION_COMMAND_STREAM_LINE_DATA 503
#define SESSION_COMMAND_STREAM_LINE_INIT 504
#define SESSION_COMMAND_STREAM_LINE_END 505

#define SESSION_RESPONSE_STREAM_RINGING 510
#define SESSION_RESPONSE_STREAM_ACCEPT 511
#define SESSION_RESPONSE_STREAM_DECLINE 512
#define SESSION_RESPONSE_STREAM_LINE_ACCEPT 513
#define SESSION_RESPONSE_STREAM_LINE_DECLINE 514

namespace CommandKeepAlive
{
	struct Command
	{
		int64_t Timestamp;
	};

	bool ParseCommand(const CowBuffer<uint8_t> buffer, Command &result);
	CowBuffer<uint8_t> BuildCommand(const Command &data);
}

namespace CommandGetHostName
{
	struct Response
	{
		String Name;
	};

	CowBuffer<uint8_t> BuildCommand();

	bool ParseResponse(const CowBuffer<uint8_t> buffer, Response &result);
	CowBuffer<uint8_t> BuildResponse(const Response &data);
};

namespace CommandRequestID
{
	struct Response
	{
		ObjectStorage::ID Id;
	};

	CowBuffer<uint8_t> BuildCommand();

	bool ParseResponse(const CowBuffer<uint8_t> buffer, Response &result);
	CowBuffer<uint8_t> BuildResponse(const Response &data);
}

namespace CommandUpdateID
{
	struct Command
	{
		ObjectStorage::ID Id;
	};

	bool ParseCommand(const CowBuffer<uint8_t> buffer, Command &result);
	CowBuffer<uint8_t> BuildCommand(const Command &command);
}

namespace CommandAddContact
{
	struct Command
	{
		String ContactName;
	};

	bool ParseCommand(const CowBuffer<uint8_t> buffer, Command &result);
	CowBuffer<uint8_t> BuildCommand(const Command &command);
}

namespace CommandUpdateContactKey
{
	struct Command
	{
		String ContactName;
		Crypto::X25519::PublicKeyContainer Key;
		uint8_t Validated;
		uint8_t Blocked;
		uint8_t SetAsDefault;
	};

	bool ParseCommand(const CowBuffer<uint8_t> buffer, Command &result);
	CowBuffer<uint8_t> BuildCommand(const Command &command);
}

namespace CommandBlockContact
{
	struct Command
	{
		String ContactName;
		uint8_t BlockStatus;
	};

	bool ParseCommand(const CowBuffer<uint8_t> buffer, Command &result);
	CowBuffer<uint8_t> BuildCommand(const Command &command);
}

/*namespace CommandListContacts
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
}*/

namespace CommandSendMessage
{
	struct Command
	{
		CowBuffer<uint8_t> Message;
	};

	bool ParseCommand(const CowBuffer<uint8_t> buffer, Command &result);
	CowBuffer<uint8_t> BuildCommand(const Command &data);
}

namespace CommandUpdateMessage
{
	struct Command
	{
		String PeerName;
		CowBuffer<uint8_t> HeaderHash;
		Message::Attribute Attr;
		uint8_t AttrValue;
	};

	bool ParseCommand(const CowBuffer<uint8_t> buffer, Command &result);
	CowBuffer<uint8_t> BuildCommand(const Command &data);
}

/*namespace CommandGetMessages
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
}*/

#endif
