#include "MessagePipe.hpp"

#include "../Common/Exception.hpp"
#include "../Message/Message.hpp"
#include "../Crypto/CryptoDefinitions.hpp"
#include "../ThirdParty/monocypher.h"

#include <cstdio>

MessagePipe::MessagePipe()
{
	_first = nullptr;
}

MessagePipe::~MessagePipe()
{
	FreeData();
}

void MessagePipe::SendMessage(const CowBuffer<uint8_t> message)
{
	Message::Header header;
	bool res = Message::GetHeader(message, header);

	if (!res) {
		THROW("Invalid message header.");
	}

	OnlineUser *user = _first;

	while (user) {
		if (!crypto_verify32(header.Destination, user->Key)) {
			user->Handler->SendMessage(message);
			return;
		}

		user = user->Next;
	}
}

SendMessageHandler *MessagePipe::GetHandler(const uint8_t *key)
{
	OnlineUser *user = _first;

	while (user) {
		if (!crypto_verify32(key, user->Key)) {
			return user->Handler;
		}

		user = user->Next;
	}

	return nullptr;
}

void MessagePipe::Register(const uint8_t *key, SendMessageHandler *handler)
{
	OnlineUser *user = new OnlineUser;
	user->Key = key;
	user->Handler = handler;
	user->Next = _first;

	_first = user;
}

void MessagePipe::Unregister(const uint8_t *key)
{
	if (!key) {
		return;
	}

	OnlineUser **user = &_first;

	while (*user) {
		if (!crypto_verify32(key, (*user)->Key)) {
			OnlineUser *tmp = *user;
			*user = (*user)->Next;
			delete tmp;
			return;
		}

		user = &((*user)->Next);
	}
}

void MessagePipe::FreeData()
{
	while (_first) {
		OnlineUser *user = _first;
		_first = _first->Next;
		delete user;
	}
}
