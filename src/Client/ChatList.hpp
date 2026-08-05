#ifndef _CHAT_LIST_HPP
#define _CHAT_LIST_HPP

#include "Root.hpp"
#include "Chat.hpp"
#include "../Message/ContactStorage.hpp"
#include "../Common/ObjectStorage.hpp"

class ChatList : public MessageEventProcessor
{
public:
	ChatList(Root *root);
	~ChatList();

	ContactStorage *GetContactStorage();

	Chat *GetCurrentChat();

	String GetCurrentChatName() override;
	String GetNextChatName(String name) override;
	String GetPreviousChatName(String name) override;

	void SwitchUp();
	void SwitchDown();

	ObjectStorage::ID GetKnownID() override;
	void SetKnownID(const ObjectStorage::ID &id) override;

	void SelectOrCreateChat(String peerName) override;

	bool HasMessage(
		String peerName,
		const ObjectStorage::ID &messageID) override;
	void DeliverMessage(
		const CowBuffer<uint8_t> message,
		Message::Attribute attr) override;
	void UpdateMessage(
		String peerName,
		const ObjectStorage::ID &messageID,
		Message::Attribute attr,
		bool value) override;

	CowBuffer<ObjectStorage::ID> ListThreads() override;
	ObjectStorage::ID GetRootMessageForThread(
		const ObjectStorage::ID &threadID) override;
	ObjectStorage::ID GetNextMessage(
		const ObjectStorage::ID &identifier) override;
	ObjectStorage::ID GetPreviousMessage(
		const ObjectStorage::ID &identifier) override;
	MessageDescriptorBase *GetMessageDescriptor(
		const ObjectStorage::ID &identifier) override;
	bool HasUnread(String chatName) override;

	void SendMessage(
		MessageDraft *draft,
		const ObjectStorage::ID &threadID) override;

private:
	Root *_root;

	class ChatContainer
	{
	public:
		ChatContainer();
		ChatContainer(String peerName);
		ChatContainer(String peerName, Chat *chat);

		Chat *GetChat()
		{
			return _chat;
		}

		String GetChatName()
		{
			return _peerName;
		}

		bool operator<(const ChatContainer &container) const;
		bool operator==(const ChatContainer &container) const;

	private:
		String _peerName;
		Chat *_chat;
	};

	Tree<ChatContainer> _chats;
	Tree<ChatContainer>::Entry *_currentChat;

	ContactStorage _contactStorage;

	void LoadChats();
	void UnloadChats();

	void LoadChat(String peerName);
};

#endif
