#ifndef _CHAT_LIST_HPP
#define _CHAT_LIST_HPP

#include "Root.hpp"
//#include "Chat.hpp"
#include "../Message/ContactStorage.hpp"
#include "../Common/ObjectStorage.hpp"

class ChatList : public MessageEventProcessor
{
public:
	ChatList(Root *root);
	~ChatList();

	ContactStorage *GetContactStorage();

//	Chat *GetCurrentChat();

	void SwitchUp();
	void SwitchDown();
	void Activate() override;
	void Deactivate() override;

	ObjectStorage::ID GetKnownID() override;
	void SetKnownID(const ObjectStorage::ID &id) override;

	void SelectOrCreateChat(String peerName) override;

private:
	Root *_root;

	/*class ChatContainer
	{
	public:
		ChatContainer();
		ChatContainer(String peerName);
		ChatContainer(String peerName, Chat *chat);

		Chat *GetChat()
		{
			return _chat;
		}

		bool operator<(const ChatContainer &container) const;
		bool operator==(const ChatContainer &container) const;

	private:
		String _peerName;
		Chat *_chat;
	};

	Tree<ChatContainer> _chats;
	Tree<ChatContainer>::Entry *_currentChat;*/

	ContactStorage _contactStorage;

	bool _currentChatIsActive;

	/*void LoadChats();
	void UnloadChats();*/

	//void LoadChat(String peerName);
};

#endif
