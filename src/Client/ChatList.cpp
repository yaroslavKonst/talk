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

	String storagePath = "storage/" +
		DataToHex(_root->PublicKey->Key, Crypto::X25519::KEY_SIZE) +
		"/storage";

	if (!FileExists(storagePath)) {
		CreateDirectory(storagePath);
	}

	_currentChat = nullptr;

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

String ChatList::GetCurrentChatName()
{
	if (!_currentChat) {
		return "";
	}

	return _currentChat->Key.GetChatName();
}

String ChatList::GetNextChatName(String name)
{
	Tree<ChatContainer>::Entry *entry = _chats.FindEntry(name);

	if (!entry) {
		return "";
	}

	entry = _chats.Next(entry);

	if (!entry) {
		return "";
	}

	return entry->Key.GetChatName();
}

String ChatList::GetPreviousChatName(String name)
{
	Tree<ChatContainer>::Entry *entry = _chats.FindEntry(name);

	if (!entry) {
		return "";
	}

	entry = _chats.Previous(entry);

	if (!entry) {
		return "";
	}

	return entry->Key.GetChatName();
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
		id.GetValuePointer(),
		(int)ObjectStorage::Constants::IDSize,
		0);
}

void ChatList::SelectOrCreateChat(String peerName)
{
	LoadChat(peerName);
	_currentChat = _chats.FindEntry(peerName);
}

bool ChatList::HasMessage(String peerName, const ObjectStorage::ID &messageID)
{
	Tree<ChatContainer>::Entry *chat = _chats.FindEntry(peerName);

	if (!chat) {
		return false;
	}

	return chat->Key.GetChat()->HasMessage(messageID);
}

void ChatList::DeliverMessage(
	const CowBuffer<uint8_t> message,
	Message::Attribute attr)
{
	Message::X25519::HeaderPointToPoint header;
	bool res = Message::X25519::ParseHeader(message, header);

	if (!res) {
		return;
	}

	if (!Message::VerifyFullUserName(header.Source)) {
		return;
	}

	if (!Message::VerifyFullUserName(header.Destination)) {
		return;
	}

	String peerName;
	String myFullName =
		_root->Conf->GetName() + "@" + _root->Conf->GetHostName();

	if (myFullName == header.Source) {
		peerName = header.Destination;
	} else if (myFullName == header.Destination) {
		peerName = header.Source;
	} else {
		return;
	}

	if (header.Source == header.Destination) {
		return;
	}

	Tree<ChatContainer>::Entry *chat = _chats.FindEntry(peerName);

	if (!chat) {
		LoadChat(peerName);
		chat = _chats.FindEntry(peerName);

		if (!_currentChat) {
			_currentChat = chat;
		}
	}

	chat->Key.GetChat()->DeliverMessage(message, attr, false);
}

void ChatList::UpdateMessage(
	String peerName,
	const ObjectStorage::ID &messageID,
	Message::Attribute attr,
	bool value)
{
	Tree<ChatContainer>::Entry *chat = _chats.FindEntry(peerName);

	if (!chat) {
		return;
	}

	chat->Key.GetChat()->UpdateMessage(messageID, attr, value);
}

CowBuffer<ObjectStorage::ID> ChatList::ListThreads()
{
	if (!_currentChat) {
		THROW("Current chat is NULL.");
	}

	return _currentChat->Key.GetChat()->ListThreads();
}

ObjectStorage::ID ChatList::GetRootMessageForThread(
	const ObjectStorage::ID &threadID)
{
	if (!_currentChat) {
		THROW("Current chat is NULL.");
	}

	return _currentChat->Key.GetChat()->GetRootMessageForThread(threadID);
}

ObjectStorage::ID ChatList::GetNextMessage(
	const ObjectStorage::ID &identifier)
{
	if (!_currentChat) {
		THROW("Current chat is NULL.");
	}

	return _currentChat->Key.GetChat()->GetNextMessage(identifier);
}

ObjectStorage::ID ChatList::GetPreviousMessage(
	const ObjectStorage::ID &identifier)
{
	if (!_currentChat) {
		THROW("Current chat is NULL.");
	}

	return _currentChat->Key.GetChat()->GetPreviousMessage(identifier);
}

MessageEventProcessor::MessageDescriptorBase *ChatList::GetMessageDescriptor(
	const ObjectStorage::ID &identifier)
{
	if (!_currentChat) {
		THROW("Current chat is NULL.");
	}

	return _currentChat->Key.GetChat()->GetMessageDescriptor(identifier);
}

bool ChatList::HasUnread(String chatName)
{
	if (!chatName.Length()) {
		if (!_currentChat) {
			THROW("Current chat is NULL.");
		}

		return _currentChat->Key.GetChat()->HasUnread();
	}

	Tree<ChatContainer>::Entry *chat = _chats.FindEntry(chatName);

	if (!chat) {
		THROW("Invalid chat is requested.");
	}

	return chat->Key.GetChat()->HasUnread();
}

void ChatList::SendMessage(MessageDraft *draft)
{
	if (!_currentChat) {
		_root->Ui->Notify("No chat is selected.");
		return;
	}

	_currentChat->Key.GetChat()->SendMessage(draft);
}

ChatList::ChatContainer::ChatContainer()
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

	_currentChat = _chats.FindSmallest();
}

void ChatList::UnloadChats()
{
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
}
