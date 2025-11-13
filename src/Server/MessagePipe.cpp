#include "MessagePipe.hpp"

#include "../Crypto/CryptoDefinitions.hpp"
#include "../ThirdParty/monocypher.h"

MessagePipe::MessagePipe()
{
	_onlineUsers = nullptr;
}

MessagePipe::~MessagePipe()
{
	FreeData();
}

void MessagePipe::SendMessage(CowBuffer<uint8_t> message)
{
	const uint8_t *key = message.Pointer() + KEY_SIZE;

	OnlineUser *user = _onlineUsers;

	while (user) {
		if (!crypto_verify32(key, user->Key)) {
			user->Handler->SendMessage(message);
			return;
		}
	}
}

void MessagePipe::FreeData()
{
	while (_onlineUsers) {
		OnlineUser *user = _onlineUsers;
		_onlineUsers = _onlineUsers->Next;
		delete user;
	}
}
