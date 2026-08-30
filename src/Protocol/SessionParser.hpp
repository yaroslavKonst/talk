#ifndef _ACTIVE_SESSION_HPP
#define _ACTIVE_SESSION_HPP

#include "../Message/Message.hpp"
#include "../Common/CowBuffer.hpp"
#include "../Common/MyString.hpp"
#include "../Common/ObjectStorage.hpp"

#define SESSION_COMMAND_KEEP_ALIVE 1
#define SESSION_COMMAND_GET_HOST_NAME 2
#define SESSION_COMMAND_GET_MAX_MESSAGE_SIZE 3
#define SESSION_COMMAND_REQUEST_ID 4
#define SESSION_COMMAND_UPDATE_ID 5
#define SESSION_COMMAND_GET_ACCOUNT_SETTINGS 6
#define SESSION_COMMAND_SET_ACCOUNT_SETTINGS 7
#define SESSION_COMMAND_ADD_CONTACT 8
#define SESSION_COMMAND_UPDATE_CONTACT_KEY 9
#define SESSION_COMMAND_BLOCK_CONTACT 10
#define SESSION_COMMAND_REMOVE_CONTACT 11
#define SESSION_COMMAND_LIST_CONTACTS 12
#define SESSION_COMMAND_OFFER_MESSAGE 13
#define SESSION_COMMAND_SEND_MESSAGE 14
#define SESSION_COMMAND_UPDATE_MESSAGE 15
#define SESSION_COMMAND_DELETE_MESSAGE 16

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

namespace CommandGetMaxMessageSize
{
	struct Response
	{
		uint64_t Value;
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

namespace CommandGetAccountSettings
{
	struct Response
	{
		bool AllowMessagesOnlyFromContactList;
		bool AllowCallsOnlyFromContactList;
		bool ShowInContactList;
	};

	CowBuffer<uint8_t> BuildCommand();

	bool ParseResponse(const CowBuffer<uint8_t> buffer, Response &result);
	CowBuffer<uint8_t> BuildResponse(const Response &data);
}

namespace CommandSetAccountSettings
{
	struct Command
	{
		bool AllowMessagesOnlyFromContactList;
		bool AllowCallsOnlyFromContactList;
		bool ShowInContactList;
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

namespace CommandRemoveContact
{
	struct Command
	{
		String ContactName;
	};

	bool ParseCommand(const CowBuffer<uint8_t> buffer, Command &result);
	CowBuffer<uint8_t> BuildCommand(const Command &command);
}

namespace CommandListContacts
{
	struct Response
	{
		struct UserData
		{
			String Name;
			Crypto::X25519::PublicKeyContainer Key;
		};

		CowBuffer<UserData> Data;
	};

	CowBuffer<uint8_t> BuildCommand();

	bool ParseResponse(const CowBuffer<uint8_t> buffer, Response &result);
	CowBuffer<uint8_t> BuildResponse(const Response &data);
}

namespace CommandOfferMessage
{
	struct Command
	{
		String PeerName;
		CowBuffer<uint8_t> HeaderHash;
	};

	struct Response
	{
		uint8_t Answer;
	};

	bool ParseCommand(const CowBuffer<uint8_t> buffer, Command &result);
	CowBuffer<uint8_t> BuildCommand(const Command &data);

	bool ParseResponse(const CowBuffer<uint8_t> buffer, Response &result);
	CowBuffer<uint8_t> BuildResponse(const Response &data);
}

namespace CommandSendMessage
{
	struct Command
	{
		Message::Attribute Attr;
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

#define SESSION_COMMAND_STREAM_INIT 100
#define SESSION_COMMAND_STREAM_RESPONSE 101
#define SESSION_COMMAND_STREAM_REQUEST 102
#define SESSION_COMMAND_STREAM_END 103
#define SESSION_COMMAND_STREAM_DATA 104

namespace CommandStreamInit
{
	struct Command
	{
		CowBuffer<uint8_t> InitRequest;
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

namespace CommandStreamResponse
{
	struct Command
	{
		CowBuffer<uint8_t> InitResponse;
	};

	bool ParseCommand(const CowBuffer<uint8_t> buffer, Command &result);
	CowBuffer<uint8_t> BuildCommand(const Command &data);
};

namespace CommandStreamRequest
{
	struct Command
	{
		CowBuffer<uint8_t> InitRequest;
	};

	struct Response
	{
		CowBuffer<uint8_t> InitResponse;
	};

	bool ParseCommand(const CowBuffer<uint8_t> buffer, Command &result);
	CowBuffer<uint8_t> BuildCommand(const Command &data);

	bool ParseResponse(const CowBuffer<uint8_t> buffer, Response &result);
	CowBuffer<uint8_t> BuildResponse(const Response &data);
}

namespace CommandStreamEnd
{
	CowBuffer<uint8_t> BuildCommand();
}

namespace CommandStreamData
{
	struct Command
	{
		CowBuffer<uint8_t> VoiceData;
	};

	bool ParseCommand(const CowBuffer<uint8_t> buffer, Command &result);
	CowBuffer<uint8_t> BuildCommand(const Command &data);
}

#endif
