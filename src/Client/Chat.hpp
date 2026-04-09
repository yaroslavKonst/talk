#ifndef _CHAT_HPP
#define _CHAT_HPP

#include "Root.hpp"
#include "../Message/Message.hpp"
#include "../Common/Tree.hpp"
#include "../Crypto/Crypto.hpp"

class MessageDecryptor
{
public:
	MessageDecryptor(
		const CowBuffer<uint8_t> *message,
		Message::Contents *contents);

	void Run();
	bool End();

private:
	const CowBuffer<uint8_t> *_message;
	int _offset;

	CowBuffer<uint8_t> _decryptedPart;

	Message::Contents *_contents;

	CryptoStreamReader _streamReader;
};

class MessageEncryptor
{
public:
	MessageEncryptor(
		const Message::Contents *contents,
		CowBuffer<uint8_t> *message);

	void Run();
	bool End();

private:
	const CowBuffer<uint8_t> *_message;
	int _offset;

	CowBuffer<uint8_t> _encryptedPart;

	Message::Contents *_contents;

	CryptoStreamWriter _streamWriter;
};

class MessageDescriptor : public QuantEventProcessor
{
public:
	MessageDescriptor(Root *root);

	Message::HeaderPointToPoint ID;

	int32_t Flags;

	Message::Contents Contents;

	MessageDecryptor *Dec;

	void ProcessQuant() override;

private:
	Root *_root;
};

class Chat
{
public:
	Chat(
		Root *root,
		String peerName);
	~Chat();

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

	Tree<MessageContainer> _messages;
	Tree<MessageContainer>::Entry *_currentMessage;

	void LoadMessages();
	void UnloadMessages();

	CowBuffer<uint8_t> EncryptMessage(
		const Message::Contents messageContents,
		const uint8_t *senderKey,
		const uint8_t *receiverKey,
		int64_t timestamp,
		int32_t index);
	Message::Contents DecryptMessage(CowBuffer<uint8_t> message);

	CowBuffer<String> _draftText;
	CowBuffer<uint8_t> _draftAttachment;

	MessageEncryptor *_enc;
};

#endif
