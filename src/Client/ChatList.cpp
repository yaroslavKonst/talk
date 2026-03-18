#include "ChatList.hpp"

#include <curses.h>

#include "TextColor.hpp"
#include "../Common/Hex.hpp"
#include "../Message/Message.hpp"

ChatList::ChatList(Root *root) :
	_contactList(session->PublicKey)
{
	_latestReceiveTime = 0;

	_root = root;
	_currentChat = nullptr;

	if (_chatCount) {
		_chatList = new Chat*[_chatCount];

		for (int i = 0; i < _chatCount; i++) {
			_chatList[i] = new Chat(
				_root,
				_contactList.GetContactKey(i),
				_notificationSystem,
				&_latestReceiveTime,
				_controls);
			_chatList[i]->SetPeerName(
				_contactList.GetNameForPresentation(i));
		}
	}
}

ChatList::~ChatList()
{
	if (_chatList) {
		for (int i = 0; i < _chatCount; i++) {
			delete _chatList[i];
		}

		delete[] _chatList;
		_chatList = nullptr;
	}
}

Chat *ChatList::GetCurrentChat()
{
	if (!_chatCount) {
		return nullptr;
	}

	return _chatList[_currentChat];
}

void ChatList::SwitchUp()
{
	--_currentChat;

	if (_currentChat < 0) {
		_currentChat = 0;
	}
}

void ChatList::SwitchDown()
{
	++_currentChat;

	if (_currentChat >= _chatCount) {
		_currentChat = _chatCount - 1;
	}

	if (_currentChat < 0) {
		_currentChat = 0;
	}
}

void ChatList::UpdateUserData(const uint8_t *key, String name)
{
	_contactList.AddContact(key, name);

	if (_contactList.GetContactCount() != _chatCount) {
		Chat **list = new Chat*[_chatCount + 1];

		if (_chatList) {
			for (int i = 0; i < _chatCount; i++) {
				list[i] = _chatList[i];
			}

			delete[] _chatList;
		}

		_chatList = list;
		++_chatCount;

		_chatList[_chatCount - 1] = new Chat(
			_session,
			_contactList.GetContactKey(_chatCount - 1),
			_notificationSystem,
			&_latestReceiveTime,
			_controls);
		_chatList[_chatCount - 1]->SetPeerName(
			_contactList.GetNameForPresentation(_chatCount - 1));
	} else {
		int index = GetUserIndexByKey(key);
		_chatList[index]->SetPeerName(
			_contactList.GetNameForPresentation(index));
	}
}

void ChatList::DeliverMessage(const CowBuffer<uint8_t> message)
{
	Message::Header header;
	bool res = Message::GetHeader(message, header);

	if (!res) {
		THROW("Invalid message header.");
	}

	const uint8_t *peerKey;

	if (!crypto_verify32(_session->PublicKey, header.Source)) {
		peerKey = header.Destination;
	} else {
		peerKey = header.Source;
	}

	for (int i = 0; i < _chatCount; i++) {
		if (!crypto_verify32(peerKey, _chatList[i]->GetPeerKey())) {
			_chatList[i]->DeliverMessage(message);
			return;
		}
	}

	UpdateUserData(peerKey, "");
	_chatList[_chatCount - 1]->DeliverMessage(message);
}

String ChatList::GetUserNameByKey(const uint8_t *key)
{
	for (int i = 0; i < _contactList.GetContactCount(); i++) {
		if (!crypto_verify32(key, _contactList.GetContactKey(i))) {
			return _contactList.GetNameForPresentation(i);
		}
	}

	return DataToHex(key, KEY_SIZE);
}

int ChatList::GetUserIndexByKey(const uint8_t *key)
{
	for (int i = 0; i < _contactList.GetContactCount(); i++) {
		if (!crypto_verify32(key, _contactList.GetContactKey(i))) {
			return i;
		}
	}

	THROW("Contact not found.");
}
