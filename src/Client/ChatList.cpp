#include "ChatList.hpp"

#include <curses.h>

#include "TextColor.hpp"
#include "../Common/Hex.hpp"

ChatList::ChatList(
	ClientSession *session,
	NotificationSystem *notificationSystem) :
	_contactList(session->PublicKey)
{
	_latestReceiveTime = 0;

	_session = session;
	_notificationSystem = notificationSystem;

	_chatCount = _contactList.GetContactCount();
	_chatList = nullptr;
	_currentChat = 0;

	if (_chatCount) {
		_chatList = new Chat*[_chatCount];

		for (int i = 0; i < _chatCount; i++) {
			_chatList[i] = new Chat(
				_session,
				_contactList.GetContactKey(i),
				_notificationSystem,
				&_latestReceiveTime);
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

void ChatList::Redraw(int rows, int columns)
{
	_rows = rows;
	_columns = columns;

	int colLimit = _columns / 4;
	int rowBase = 3;
	int rowLimit = _rows - 2;

	int start;
	int end;

	if (_chatCount <= rowLimit - rowBase) {
		start = 0;
		end = _chatCount;
	} else {
		start = _currentChat - (rowLimit - rowBase) / 2;

		if (start < 0) {
			start = 0;
		}

		end = start + (rowLimit - rowBase);

		if (end > _chatCount) {
			end = _chatCount;
		}
	}

	for (int i = start; i < end; i++) {
		String name = _contactList.GetNameForPresentation(i);

		bool hasUnread = _chatList[i]->HasUnread();
		bool hasUnsent = _chatList[i]->HasUnsent();

		if (hasUnread || hasUnsent) {
			name = String("!") + name;
		} else {
			name = String(" ") + name;
		}

		if (name.Length() > colLimit) {
			name = name.Substring(0, colLimit);
		}

		int attrs = 0;

		if (hasUnread || hasUnsent) {
			attrs |= COLOR_PAIR(YELLOW_TEXT);
		}

		if (i == _currentChat) {
			attrs |= A_BOLD;
		}

		if (attrs) {
			attrset(attrs);
		}

		move(rowBase + i - start, 0);
		addstr(name.CStr());
		attrset(COLOR_PAIR(DEFAULT_TEXT));
	}

	move(rowBase + _currentChat - start, 0);
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
			&_latestReceiveTime);
		_chatList[_chatCount - 1]->SetPeerName(
			_contactList.GetNameForPresentation(_chatCount - 1));
	} else {
		int index = GetUserIndexByKey(key);
		_chatList[index]->SetPeerName(
			_contactList.GetNameForPresentation(index));
	}
}

void ChatList::DeliverMessage(CowBuffer<uint8_t> message)
{
	const uint8_t *peerKey;

	if (!crypto_verify32(_session->PublicKey, message.Pointer())) {
		peerKey = message.Pointer() + KEY_SIZE;
	} else {
		peerKey = message.Pointer();
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
