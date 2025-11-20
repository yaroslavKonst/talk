#ifndef _CHAT_HPP
#define _CHAT_HPP

#include "NotificationSystem.hpp"
#include "../Protocol/ClientSession.hpp"
#include "../Message/MessageStorage.hpp"
#include "../Message/AttributeStorage.hpp"

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

	void SetPeerName(String name)
	{
		_peerName = name;
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
	String _peerName;

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

#endif
