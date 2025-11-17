#include "MessagePipe.hpp"

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

void MessagePipe::SendMessage(CowBuffer<uint8_t> message)
{
	const uint8_t *key = message.Pointer() + KEY_SIZE;

	OnlineUser *user = _first;

	while (user) {
		if (!crypto_verify32(key, user->Key)) {
			user->Handler->SendMessage(message);
			// DEBUG
			printf("Message sent.\n");
			return;
		}

		user = user->Next;
	}
}

void MessagePipe::Register(const uint8_t *key, SendMessageHandler *handler)
{
	OnlineUser *user = new OnlineUser;
	user->Key = key;
	user->Handler = handler;
	user->Next = _first;

	_first = user;

	// DEBUG
	printf("New user registered.\n");
}

void MessagePipe::Unregister(const uint8_t *key)
{
	// DEBUG
	printf("User asked for unregister.\n");

	OnlineUser **user = &_first;

	while (*user) {
		if (!crypto_verify32(key, (*user)->Key)) {
			OnlineUser *tmp = *user;
			*user = (*user)->Next;
			delete tmp;
			printf("User unregistered.\n");
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
