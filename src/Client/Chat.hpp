#ifndef _CHAT_HPP
#define _CHAT_HPP

#include "NotificationSystem.hpp"
#include "../Protocol/ClientSession.hpp"
#include "../Message/MessageStorage.hpp"
#include "../Message/AttributeStorage.hpp"

struct MessageContents
{
	String Text;
	CowBuffer<uint8_t> Attachment;

	bool IsEmpty() const
	{
		return !Text.Length() && !Attachment.Size();
	}

	// Parser.
	enum EntryType
	{
		EntryTypeText = 1,
		EntryTypeData = 2
	};

	CowBuffer<uint8_t> Build() const
	{
		CowBuffer<uint8_t> text;
		CowBuffer<uint8_t> data;

		if (Text.Length()) {
			text.Resize(sizeof(int32_t) * 2 + Text.Length());
			*text.SwitchType<int32_t>() = EntryTypeText;
			*text.SwitchType<int32_t>(sizeof(int32_t)) =
				Text.Length();

			memcpy(
				text.Pointer(sizeof(int32_t) * 2),
				Text.CStr(),
				Text.Length());
		}

		if (Attachment.Size()) {
			data.Resize(sizeof(int32_t) * 2 + Attachment.Size());
			*data.SwitchType<int32_t>() = EntryTypeData;
			*data.SwitchType<int32_t>(sizeof(int32_t)) =
				Attachment.Size();

			memcpy(
				data.Pointer(sizeof(int32_t) * 2),
				Attachment.Pointer(),
				Attachment.Size());
		}

		return text.Concat(data);
	}

	void Parse(const CowBuffer<uint8_t> data)
	{
		if (!data.Size()) {
			return;
		}

		unsigned int offset = 0;

		while (offset < data.Size()) {
			int type = ' ';

			if (data.Size() - offset >= sizeof(int32_t)) {
				type = *data.SwitchType<int32_t>(offset);
			}

			if (type == EntryTypeText) {
				int size = *data.SwitchType<int32_t>(
					offset + sizeof(int32_t));

				offset += sizeof(int32_t) * 2;
				Text.Clear();

				for (int i = 0; i < size; i++) {
					Text += data[offset];
					++offset;
				}
			} else if (type == EntryTypeData) {
				int size = *data.SwitchType<int32_t>(
					offset + sizeof(int32_t));

				offset += sizeof(int32_t) * 2;
				Attachment.Resize(size);

				for (int i = 0; i < size; i++) {
					Attachment[i] = data[offset];
					++offset;
				}
			} else {
				Text.Clear();

				while (offset < data.Size()) {
					Text += data[offset];
					++offset;
				}
			}
		}
	}
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

	MessageContents DecryptedData;

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
	void MarkRead(int messageIndex);

	bool HasAttachment();
	CowBuffer<uint8_t> ExtractAttachment();

	void AddAttachment(const CowBuffer<uint8_t> attachment);
	void ClearAttachment();

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
		const MessageContents messageContents,
		const uint8_t *senderKey,
		const uint8_t *receiverKey,
		int64_t timestamp,
		int32_t index);
	MessageContents DecryptMessage(CowBuffer<uint8_t> message);

	void SendMessage();

	int _utf8ExpectedSize;
	String _utf8Buffer;

	String _draft;
	String _draftSuffix;
	CowBuffer<uint8_t> _draftAttachment;

	NotificationSystem *_notificationSystem;

	int64_t *_latestReceiveTime;
};

#endif
