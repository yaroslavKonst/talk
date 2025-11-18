#ifndef _UI_HPP
#define _UI_HPP

#include <curses.h>

#include "../Protocol/ClientSession.hpp"
#include "../Message/ContactStorage.hpp"
#include "../Message/MessageStorage.hpp"
#include "../Message/AttributeStorage.hpp"

class NotifyRedrawHandler
{
public:
	virtual ~NotifyRedrawHandler()
	{ }

	virtual void NotifyRedraw() = 0;
};

class NotificationSystem
{
public:
	NotificationSystem(NotifyRedrawHandler *handler);
	~NotificationSystem();

	void Notify(String message);

	void Redraw();
	bool ProcessEvent(int event);

private:
	struct Notification
	{
		Notification *Next;
		String Message;
	};

	Notification *_first;
	Notification *_last;

	NotifyRedrawHandler *_handler;
};

class MessageDescriptor
{
public:
	MessageDescriptor(AttributeStorage *attrStorage);

	MessageDescriptor *Next;

	CowBuffer<uint8_t> Message;

	bool Read;
	bool Sent;
	bool SendFailure;
	bool SendInProcess;

	String Text;

	void SetRead(bool value);
	void SetSent(bool value);
	void SetSendFailure(bool value);

private:
	AttributeStorage *_attributeStorage;

	void SaveAttributes();
};

class Chat
{
public:
	Chat(
		ClientSession *session,
		const uint8_t *peerKey,
		NotificationSystem *notificationSystem,
		int64_t *latestReceiveTime);
	~Chat();

	const uint8_t *GetPeerKey()
	{
		return _peerKey;
	}

	void Redraw(int rows, int columns);

	bool HasUnread();
	bool HasUnsent();

	bool Typing();

	void StartTyping();
	void ProcessTyping(int event);

	void SwitchUp();
	void SwitchDown();

	void DeliverMessage(CowBuffer<uint8_t> message);

	void MarkRead();

private:
	ClientSession *_session;

	int _rows;
	int _columns;

	void RedrawMessageWindow();
	void RedrawTextWindow();
	CowBuffer<String> MakeMultiline(String text, int limit);

	static int64_t _LastLoadedTime;

	const uint8_t *_peerKey;

	bool _typing;

	MessageStorage _messageStorage;
	AttributeStorage _attributeStorage;

	MessageDescriptor *_last;
	int _loadedMessages;
	int _currentMessage;

	void LoadMessages(int count);
	void UnloadMessages();

	CowBuffer<uint8_t> EncryptMessage(
		String text,
		const uint8_t *senderKey,
		const uint8_t *receiverKey,
		int64_t timestamp,
		int32_t index);
	String DecryptMessage(CowBuffer<uint8_t> message);

	void SendMessage();

	String _draft;

	NotificationSystem *_notificationSystem;

	int64_t *_latestReceiveTime;
};

class ChatList
{
public:
	ChatList(
		ClientSession *session,
		NotificationSystem *notificationSystem);
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
};

class Screen
{
public:
	Screen(ClientSession *session);
	virtual ~Screen();

	virtual Screen *ProcessEvent(int event) = 0;
	virtual void Redraw() = 0;
	void ProcessResize();

protected:
	int _rows;
	int _columns;

	ClientSession *_session;

	void ClearScreen();
};

class PasswordScreen : public Screen
{
public:
	PasswordScreen(ClientSession *session);

	void Redraw() override;
	Screen *ProcessEvent(int event) override;

private:
	String _password;

	void GenerateKeys();
};

class LoginScreen : public Screen
{
public:
	LoginScreen(ClientSession *session);

	void Redraw() override;
	Screen *ProcessEvent(int event) override;

private:
	bool _writingIp;
	bool _writingPort;
	bool _writingKey;

	String _ip;
	String _port;
	String _serverKeyHex;
};

class WorkScreen :
	public Screen,
	public MessageProcessor,
	public NotifyRedrawHandler
{
public:
	WorkScreen(ClientSession *session);
	~WorkScreen();

	void Redraw() override;
	Screen *ProcessEvent(int event) override;

	void NotifyDelivery(void *userPointer, int32_t status) override;
	void DeliverMessage(CowBuffer<uint8_t> message) override;
	void UpdateUserData(const uint8_t *key, String name) override;
	int64_t GetLatestReceiveTimestamp() override
	{
		return _chatList.GetLatestTimestamp();
	}

	void NotifyRedraw() override
	{
		Redraw();
	}

private:
	NotificationSystem _notificationSystem;

	Screen *_overlay;

	void Connect();

	ChatList _chatList;
	Chat *_activeChat;

	void ProcessChatListEvent(int event);
	void ProcessChatScreenEvent(int event);
};

class UI
{
public:
	UI(ClientSession *session);
	~UI();

	bool ProcessEvent();
	void ProcessResize();

	void Disconnect();

private:
	Screen *_screen;

	ClientSession *_session;
};

#endif
