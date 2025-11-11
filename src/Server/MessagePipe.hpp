#ifndef _MESSAGE_PIPE_HPP
#define _MESSAGE_PIPE_HPP

#include "../Common/CowBuffer.hpp"

// Message structure
// | sender key | receiver key | timestamp (int64_t) | encrypted data |

class SendMessageHandler
{
public:
	virtual void SendMessage(CowBuffer<uint8_t> message) = 0;
};

class MessagePipe
{
public:
	MessagePipe();
	~MessagePipe();

	void SendMessage(CowBuffer<uint8_t> message);

	void Register(
		const uint8_t *key,
		SendMessageHandler *handler);

	void Unregister(const uint8_t *key);

private:
	struct OnlineUser
	{
		OnlineUser *Next;

		const uint8_t *Key;
		SendMessageHandler *Handler;
	};

	OnlineUser *_onlineUsers;

	void FreeData();
};

#endif
