#ifndef _CHAT_HPP
#define _CHAT_HPP

#include "NotificationSystem.hpp"
#include "../Protocol/ClientSession.hpp"
#include "../Message/MessageStorage.hpp"
#include "../Message/AttributeStorage.hpp"
#include "../Common/Tree.hpp"

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

	CowBuffer<uint8_t> Build() const;
	void Parse(const CowBuffer<uint8_t> data);
};

class MessageDecryptor
{
public:
	MessageDecryptor(
		const CowBuffer<uint8_t> *message,
		MessageContents *contents);

	void Run();
	bool End();

private:
	const CowBuffer<uint8_t> *_message;
	int _offset;

	CowBuffer<uint8_t> _decryptedPart;

	MessageContents *_contents;

	CryptoStreamReader _streamReader;
};

class MessageEncryptor
{
public:
	MessageEncryptor(
		const MessageContents *contents,
		CowBuffer<uint8_t> *message);

	void Run();
	bool End();

private:
	const CowBuffer<uint8_t> *_message;
	int _offset;

	CowBuffer<uint8_t> _encryptedPart;

	MessageContents *_contents;
};

class MessageDescriptor : public QuantEventProcessor
{
public:
	MessageDescriptor(
		MessageStorage *msgStorage,
		AttributeStorage *attrStorage,
		Root *root);

	Message::MessageID ID;
	CowBuffer<uint8_t> Message;

	bool Read;
	bool Sent;
	bool SendFailure;
	bool SendInProcess;

	MessageContents DecryptedData;

	void SetRead(bool value);
	void SetSent(bool value);
	void SetSendFailure(bool value);

	MessageDecryptor *Dec;
	MessageEncryptor *Enc;

	void ProcessQuant() override;

private:
	MessageStorage *_messageStorage;
	AttributeStorage *_attributeStorage;
	Root *_root;

	void SaveAttributes();
};

class Chat
{
public:
	Chat(Root *root, const uint8_t *peerKey, int64_t *latestReceiveTime);
	~Chat();

	const uint8_t *GetPeerKey()
	{
		return _peerKey;
	}

	bool HasUnread();

	void SwitchUp();
	void SwitchDown();

	void MoveLeft();
	void MoveRight();

	void AddChar(int c);

	void SendMessage();

	void DeliverMessage(CowBuffer<uint8_t> message);

	void MarkReadCurrentMessage();

	bool HasAttachment();
	CowBuffer<uint8_t> ExtractAttachment();

	void AddAttachment(const CowBuffer<uint8_t> attachment);
	void ClearAttachment();

private:
	struct MessageContainer
	{
		MessageDescriptor *Descriptor;

		MessageContainer(MessageDescriptor *descr)
		{
			Descriptor = descr;
		}

		bool operator==(const MessageContainer &c) const
		{
			return Descriptor->ID == c.Descriptor->ID;
		}

		bool operator<(const MessageContainer &c) const
		{
			return Descriptor->ID < c.Descriptor->ID;
		}
	};

	Root *_root;

	const uint8_t *_peerKey;

	MessageStorage _messageStorage;
	AttributeStorage _attributeStorage;

	int64_t *_latestReceiveTime;

	Tree<MessageContainer> _messages;
	Tree<MessageContainer>::Entry *_currentMessage;

	void LoadMessages();
	void UnloadMessages();

	CowBuffer<uint8_t> EncryptMessage(
		const MessageContents messageContents,
		const uint8_t *senderKey,
		const uint8_t *receiverKey,
		int64_t timestamp,
		int32_t index);
	MessageContents DecryptMessage(CowBuffer<uint8_t> message);

	int _utf8ExpectedSize;
	String _utf8Buffer;

	String _draft;
	String _draftSuffix;
	CowBuffer<uint8_t> _draftAttachment;
};

#endif
