#ifndef _CHAT_LIST_HPP
#define _CHAT_LIST_HPP

#include "Root.hpp"
#include "Chat.hpp"
#include "../Message/ContactStorage.hpp"

class ChatContainer
{
public:
	ChatContainer(const uint8_t *peerKey, Chat *chat);

	bool operator==(const ChatContainer &container) const;
	bool operator<(const ChatContainer &container) const;

private:
	const uint8_t *_peerKey;
	Chat *_chat;
};

class ChatList : public MessageEventProcessor
{
public:
	ChatList(Root *root);
	~ChatList();

	Chat *GetCurrentChat();

	void SwitchUp();
	void SwitchDown();
	void Activate();
	void Deactivate();

	void UpdateUserData(const uint8_t *key, String name);
	void DeliverMessage(CowBuffer<uint8_t> message);

	String GetUserNameByKey(const uint8_t *key);
	int GetUserIndexByKey(const uint8_t *key);

private:
	Root *_root;

	Tree<ChatContainer> _chats;
	Tree<ChatContainer>::Entry *_currentChat;

	ContactStorage _contactList;

	int64_t _latestReceiveTime;
};

#endif
