#ifndef _MESSAGE_PIPE_HPP
#define _MESSAGE_PIPE_HPP

#include "../Common/CowBuffer.hpp"

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

	void Register(const uint8_t *key, SendMessageHandler *handler);
	void Unregister(const uint8_t *key);

private:
	struct OnlineUser
	{
		OnlineUser *Next;

		const uint8_t *Key;
		SendMessageHandler *Handler;
	};

	OnlineUser *_first;

	void FreeData();
};

#endif
