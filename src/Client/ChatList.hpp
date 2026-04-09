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

	void SwitchUp();
	void SwitchDown();
	void Activate();
	void Deactivate();

private:
	Root *_root;

	class ChatContainer
	{
	public:
		ChatContainer(String peerName, Chat *chat);

		Chat *GetChat()
		{
			return _chat;
		}

		bool operator==(const ChatContainer &container) const;
		bool operator<(const ChatContainer &container) const;

	private:
		String _peerName;
		Chat *_chat;
	};

	Tree<ChatContainer> _chats;
	Tree<ChatContainer>::Entry *_currentChat;

	ContactStorage _contactStorage;
	ObjectStorage _objectStorage;

	bool _currentChatIsActive;

	void LoadChats();
	void UnloadChats();
};

#endif
