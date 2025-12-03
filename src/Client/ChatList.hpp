#ifndef _CHAT_LIST_HPP
#define _CHAT_LIST_HPP

#include "Chat.hpp"
#include "../Protocol/ClientSession.hpp"
#include "../Message/ContactStorage.hpp"

class ChatList
{
public:
	ChatList(
		ClientSession *session,
		NotificationSystem *notificationSystem,
		ControlStorage *controls);
	~ChatList();

	void Redraw(int rows, int columns);

	Chat *GetCurrentChat();

	void SwitchUp();
	void SwitchDown();

	void UpdateUserData(const uint8_t *key, String name);
	void DeliverMessage(CowBuffer<uint8_t> message);

	int64_t GetLatestTimestamp()
	{
		return _latestReceiveTime;
	}

	String GetUserNameByKey(const uint8_t *key);
	int GetUserIndexByKey(const uint8_t *key);

private:
	ClientSession *_session;

	Chat **_chatList;
	int _chatCount;
	int _currentChat;

	int _rows;
	int _columns;

	ContactStorage _contactList;

	NotificationSystem *_notificationSystem;

	int64_t _latestReceiveTime;

	ControlStorage *_controls;
};

#endif
