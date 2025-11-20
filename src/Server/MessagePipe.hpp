#ifndef _MESSAGE_PIPE_HPP
#define _MESSAGE_PIPE_HPP

#include "../Common/CowBuffer.hpp"

class SendMessageHandler
{
public:
	virtual void SendMessage(CowBuffer<uint8_t> message) = 0;

	virtual bool InVoice() = 0;
	virtual void SendVoiceFrame(CowBuffer<uint8_t> frame) = 0;
	virtual void StartVoice(
		const uint8_t *peerKey,
		int64_t timestamp,
		SendMessageHandler *handler) = 0;
	virtual void AcceptVoice() = 0;
	virtual void DeclineVoice() = 0;
	virtual void EndVoice() = 0;
};

class MessagePipe
{
public:
	MessagePipe();
	~MessagePipe();

	void SendMessage(CowBuffer<uint8_t> message);
	SendMessageHandler *GetHandler(const uint8_t *key);

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
