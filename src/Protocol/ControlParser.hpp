#ifndef _CONTROL_PARSER_HPP
#define _CONTROL_PARSER_HPP

#include "../Common/CowBuffer.hpp"
#include "../Common/MyString.hpp"
#include "../Crypto/Crypto.hpp"

#define TALKD_SOCKET_NAME "talkd.socket"

#define OK 200

// Result codes.
#define ERROR 100
#define ERROR_UNKNOWN_COMMAND 101
#define ERROR_TOO_SHORT 102
#define ERROR_INVALID_SIZE 103
#define ERROR_INVALID_USER 104
#define ERROR_USER_EXISTS 105
#define ERROR_INVALID_IP 106
#define ERROR_UNSUPPORTED_OPTION 107

// Commands.
#define COMMAND_SHUTDOWN 1
#define COMMAND_GET_PUBLIC_KEY 2
#define COMMAND_RELOAD_CONFIG 3

#define COMMAND_ADD_USER 4
#define COMMAND_REMOVE_USER 5
#define COMMAND_LIST_USERS 6

#define COMMAND_FAILBAN_LIST_BANNED 7
#define COMMAND_FAILBAN_BAN 8
#define COMMAND_FAILBAN_UNBAN 9

namespace CommandAddUser
{
	struct Request
	{
		String Name;
		Crypto::X25519::PublicKeyContainer Key;
	};

	struct Response
	{
		int32_t Code;
	};

	bool ParseRequest(const CowBuffer<uint8_t> buffer, Request &request);
	CowBuffer<uint8_t> BuildRequest(const Request &request);

	bool ParseResponse(const CowBuffer<uint8_t> buffer, Response &response);
	CowBuffer<uint8_t> BuildResponse(const Response &response);
}

namespace CommandRemoveUser
{
	struct Request
	{
		String Name;
	};

	struct Response
	{
		int32_t Code;
	};

	bool ParseRequest(const CowBuffer<uint8_t> buffer, Request &request);
	CowBuffer<uint8_t> BuildRequest(const Request &request);

	bool ParseResponse(const CowBuffer<uint8_t> buffer, Response &response);
	CowBuffer<uint8_t> BuildResponse(const Response &response);
}

namespace CommandListUsers
{
	enum Flag : int32_t
	{
		ShowKeys = 0x00000001
	};

	struct Request
	{
		int32_t Flags;
	};

	struct Response
	{
		struct UserData
		{
			String Name;
			Crypto::X25519::PublicKeyContainer Key;
		};

		int32_t Code;
		int32_t Flags;
		CowBuffer<UserData> Data;
	};

	bool ParseRequest(const CowBuffer<uint8_t> buffer, Request &request);
	CowBuffer<uint8_t> BuildRequest(const Request &request);

	bool ParseResponse(const CowBuffer<uint8_t> buffer, Response &response);
	CowBuffer<uint8_t> BuildResponse(const Response &response);
}

namespace CommandFailBanListBanned
{
	struct Response
	{
		int32_t Code;
		CowBuffer<uint32_t> BannedIPList;
	};

	bool ParseResponse(const CowBuffer<uint8_t> buffer, Response &response);
	CowBuffer<uint8_t> BuildResponse(const Response &response);
}

namespace CommandFailBanBan
{
	struct Request
	{
		uint32_t IP;
	};

	struct Response
	{
		int32_t Code;
	};

	bool ParseRequest(const CowBuffer<uint8_t> buffer, Request &request);
	CowBuffer<uint8_t> BuildRequest(const Request &request);

	bool ParseResponse(const CowBuffer<uint8_t> buffer, Response &response);
	CowBuffer<uint8_t> BuildResponse(const Response &response);
}

namespace CommandFailBanUnban
{
	struct Request
	{
		uint32_t IP;
	};

	struct Response
	{
		int32_t Code;
	};

	bool ParseRequest(const CowBuffer<uint8_t> buffer, Request &request);
	CowBuffer<uint8_t> BuildRequest(const Request &request);

	bool ParseResponse(const CowBuffer<uint8_t> buffer, Response &response);
	CowBuffer<uint8_t> BuildResponse(const Response &response);
}

#endif
