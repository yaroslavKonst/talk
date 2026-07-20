#include "ChatList.hpp"

#include <curses.h>

#include "TextColor.hpp"
#include "../Common/Hex.hpp"
#include "../Common/File.hpp"
#include "../Common/BinaryFile.hpp"
#include "../Message/Message.hpp"

#define STORED_OBJECT_ID "/server_ref"

ChatList::ChatList(Root *root) :
	_contactStorage(
		"storage/" + DataToHex(
			root->PublicKey->Key,
			Crypto::X25519::KEY_SIZE))
{
	_root = root;
	//_currentChat = nullptr;
	_currentChatIsActive = false;

	//LoadChats();
}

ChatList::~ChatList()
{
	//UnloadChats();
}

ContactStorage *ChatList::GetContactStorage()
{
	return &_contactStorage;
}

/*Chat *ChatList::GetCurrentChat()
{
	if (!_currentChat) {
		return nullptr;
	}

	return _currentChat->Key.GetChat();
}*/

void ChatList::SwitchUp()
{
	/*if (!_currentChat) {
		_currentChat = _chats.FindBiggest();
		return;
	}

	Tree<ChatContainer>::Entry *newChat = _chats.Previous(_currentChat);

	if (newChat) {
		_currentChat = newChat;
	}*/
}

void ChatList::SwitchDown()
{
	/*if (!_currentChat) {
		_currentChat = _chats.FindSmallest();
		return;
	}

	Tree<ChatContainer>::Entry *newChat = _chats.Next(_currentChat);

	if (newChat) {
		_currentChat = newChat;
	}*/
}

void ChatList::Activate()
{
	/*if (_currentChat) {
		_currentChatIsActive = true;
	}*/
}

void ChatList::Deactivate()
{
	_currentChatIsActive = false;
}

ObjectStorage::ID ChatList::GetKnownID()
{
	String path = "storage/" +
		DataToHex(
			_root->PublicKey->Key,
			Crypto::X25519::KEY_SIZE) +
		STORED_OBJECT_ID;

	if (!FileExists(path)) {
		return ObjectStorage::ID();
	}

	CowBuffer<uint8_t> id((int)ObjectStorage::Constants::IDSize);

	BinaryFile file(path, false);
	file.Read<uint8_t>(id.Pointer(), id.Size(), 0);
	return ObjectStorage::ID(id.Pointer());
}

void ChatList::SetKnownID(const ObjectStorage::ID &id)
{
	String path = "storage/" +
		DataToHex(
			_root->PublicKey->Key,
			Crypto::X25519::KEY_SIZE) +
		STORED_OBJECT_ID;

	BinaryFile file(path, true);
	file.Write<uint8_t>(
		id.GetValue(),
		(int)ObjectStorage::Constants::IDSize,
		0);
}

void ChatList::SelectOrCreateChat(String peerName)
{
	/*LoadChat(peerName);
	_currentChat = _chats.FindEntry(peerName);*/
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

/*ChatList::ChatContainer::ChatContainer()
{
	_chat = nullptr;
}

ChatList::ChatContainer::ChatContainer(String peerName)
{
	_peerName = peerName;
	_chat = nullptr;
}

ChatList::ChatContainer::ChatContainer(String peerName, Chat *chat)
{
	_peerName = peerName;
	_chat = chat;
}

bool ChatList::ChatContainer::operator<(const ChatContainer &container) const
{
	return _peerName < container._peerName;
}

bool ChatList::ChatContainer::operator==(const ChatContainer &container) const
{
	return _peerName == container._peerName;
}

void ChatList::LoadChats()
{
	CowBuffer<String> peerList = ListDirectory(
		"storage/" + DataToHex(
			_root->PublicKey->Key,
			Crypto::X25519::KEY_SIZE) +
		"/storage");

	for (uint64_t i = 0; i < peerList.Size(); i++) {
		LoadChat(peerList[i]);
	}
}

void ChatList::UnloadChats()
{
	_currentChatIsActive = false;
	_currentChat = _chats.FindSmallest();

	while (_currentChat) {
		delete _currentChat->Key.GetChat();
		Tree<ChatContainer>::Entry *nextChat =
			_chats.Next(_currentChat);

		_chats.RemoveEntry(_currentChat);
		_currentChat = nextChat;
	}
}

void ChatList::LoadChat(String peerName)
{
	if (_chats.FindEntry(peerName)) {
		return;
	}

	Chat *chat = new Chat(_root, peerName);
	_chats.AddEntry(ChatContainer(peerName, chat));
}*/
