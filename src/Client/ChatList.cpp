#include "ChatList.hpp"

#include <curses.h>

#include "TextColor.hpp"
#include "../Common/Hex.hpp"
#include "../Message/Message.hpp"

ChatList::ChatList(Root *root) :
	_contactStorage("storage/" + DataToHex(root->PublicKey, KEY_SIZE)),
	_objectStorage(
		"storage/" + DataToHex(root->PublicKey, KEY_SIZE) + "/storage",
		root->Dispatcher)
{
	_root = root;
	_currentChat = nullptr;
	_currentChatIsActive = false;

	LoadChats();
}

ChatList::~ChatList()
{
	UnloadChats();
}

ContactStorage *ChatList::GetContactStorage()
{
	return &_contactStorage;
}

Chat *ChatList::GetCurrentChat()
{
	if (!_currentChat) {
		return nullptr;
	}

	return _currentChat->Key.GetChat();
}

void ChatList::SwitchUp()
{
	if (!_currentChat) {
		_currentChat = _chats.FindBiggest();
		return;
	}

	Tree<ChatContainer>::Entry *newChat = _chats.Previous(_currentChat);

	if (newChat) {
		_currentChat = newChat;
	}
}

void ChatList::SwitchDown()
{
	if (!_currentChat) {
		_currentChat = _chats.FindSmallest();
		return;
	}

	Tree<ChatContainer>::Entry *newChat = _chats.Next(_currentChat);

	if (newChat) {
		_currentChat = newChat;
	}
}

void ChatList::Activate()
{
	if (_currentChat) {
		_currentChatIsActive = true;
	}
}

void ChatList::Deactivate()
{
	_currentChatIsActive = false;
}

/*void ChatList::DeliverMessage(const CowBuffer<uint8_t> message)
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
}*/

void ChatList::LoadChats()
{
}

void ChatList::UnloadChats()
{
}
