#ifndef _CHAT_HPP
#define _CHAT_HPP

#include "Root.hpp"
#include "TextEditor.hpp"
#include "MessageDescriptor.hpp"
#include "../Message/Message.hpp"
#include "../Crypto/Crypto.hpp"

class Chat : public QuantEventProcessor
{
public:
	Chat(
		Root *root,
		String peerName);
	~Chat();

	bool HasMessage(const ObjectStorage::ID &messageID);
	bool HasUnread();

	void SendMessage(MessageDraft *draft);

	void DeliverMessage(
		const CowBuffer<uint8_t> message,
		Message::Attribute attr,
		bool local);
	void UpdateMessage(
		const ObjectStorage::ID &messageID,
		Message::Attribute attr,
		bool value);

	void ProcessQuant() override;

	CowBuffer<ObjectStorage::ID> ListThreads();
	ObjectStorage::ID GetRootMessageForThread(
		const ObjectStorage::ID &threadID);

	ObjectStorage::ID GetNextMessage(
		const ObjectStorage::ID &identifier);
	ObjectStorage::ID GetPreviousMessage(
		const ObjectStorage::ID &identifier);

	MessageDescriptor *GetMessageDescriptor(
		const ObjectStorage::ID &identifier);

private:
	struct MessageNode
	{
		MessageNode *Next;
		MessageNode *Previous;
		MessageDescriptor *Descriptor;

		MessageNode(MessageDescriptor *descr)
		{
			Next = nullptr;
			Previous = nullptr;
			Descriptor = descr;
		}

		~MessageNode()
		{
			if (Descriptor) {
				delete Descriptor;
				Descriptor = nullptr;
			}
		}
	};

	struct MessageTreeEntry
	{
		ObjectStorage::ID Identifier;
		MessageNode *Node;

		bool operator==(const MessageTreeEntry &e) const;
		bool operator<(const MessageTreeEntry &e) const;

		MessageTreeEntry()
		{
			Node = nullptr;
		}

		MessageTreeEntry(MessageNode *node)
		{
			Node = node;
			Identifier = node->Descriptor->Identifier;
		}

		MessageTreeEntry(const ObjectStorage::ID &id)
		{
			Node = nullptr;
			Identifier = id;
		}
	};

	struct UnreadMessageTreeEntry
	{
		MessageNode *Node;

		ObjectStorage::ID Identifier;
		int64_t Timestamp;
		int32_t Index;

		bool operator==(const UnreadMessageTreeEntry &e) const;
		bool operator<(const UnreadMessageTreeEntry &e) const;

		UnreadMessageTreeEntry()
		{
			Node = nullptr;
			Timestamp = 0;
			Index = 0;
		}

		UnreadMessageTreeEntry(MessageNode *node)
		{
			Node = node;
			Identifier = node->Descriptor->Identifier;
			Timestamp = node->Descriptor->Header.Timestamp;
			Index = node->Descriptor->Header.Index;
		}

		UnreadMessageTreeEntry(
			const ObjectStorage::ID &id,
			int64_t timestamp,
			int32_t index)
		{
			Node = nullptr;
			Identifier = id;
			Timestamp = timestamp;
			Index = index;
		}

		UnreadMessageTreeEntry(int64_t timestamp, int32_t index)
		{
			Node = nullptr;
			Timestamp = timestamp;
			Index = index;
		}
	};

	Root *_root;
	ObjectStorage _objectStorage;
	String _peerName;

#warning May be unused.
	ObjectStorage::ID _currentThreadID;
	MessageNode *_currentMessage;
	int64_t _currentMessageLine;

	Tree<MessageTreeEntry> _messagesByID;
	Tree<UnreadMessageTreeEntry> _unreadMessages;

	void LoadMessages();
	void UnloadMessages();

	void AddMessageToThread(
		ObjectStorage::ID messageID,
		const Message::X25519::HeaderPointToPoint &header,
		const CowBuffer<uint8_t> message,
		Message::Attribute attrs);

	MessageEncryptor *_enc;
	CowBuffer<uint8_t> _encMessage;
	void *_encLock;

	MessageDraft *_draftPtr;
};

#endif
