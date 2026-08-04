#ifndef _PROCESSORS_HPP
#define _PROCESSORS_HPP

#include "MessageDraft.hpp"
#include "../Message/ContactStorage.hpp"
#include "../Protocol/SessionParser.hpp"
#include "../Common/CowBuffer.hpp"
#include "../Common/MyString.hpp"
#include "../Common/ObjectStorage.hpp"
#include "../Crypto/Crypto.hpp"

// Processors are objects that can process events of certain types.

// Text chats.
class MessageEventProcessor
{
public:
	class MessageDescriptorBase
	{
	public:
		virtual ~MessageDescriptorBase()
		{ }

#warning TODO: implementation.
		//virtual const Message::HeaderPointToPoint &GetHeader() = 0;

		virtual bool HasContents() = 0;
		virtual const Message::Contents &GetContents() = 0;

		virtual void RunDecryption() = 0;
		virtual bool DecryptionInProgress() = 0;
		virtual bool DecryptionFailure() = 0;

		virtual void Clear() = 0;
	};

	virtual ~MessageEventProcessor()
	{ }

	virtual ContactStorage *GetContactStorage() = 0;
	virtual ObjectStorage::ID GetKnownID() = 0;
	virtual void SetKnownID(const ObjectStorage::ID &id) = 0;

	virtual void SelectOrCreateChat(String peerName) = 0;
	virtual bool HasMessage(
		String peerName,
		const ObjectStorage::ID &messageID) = 0;
	virtual void DeliverMessage(
		const CowBuffer<uint8_t> message,
		Message::Attribute attr) = 0;
	virtual void UpdateMessage(
		String peerName,
		const ObjectStorage::ID &messageID,
		Message::Attribute attr,
		bool value) = 0;

	virtual String GetCurrentChatName() = 0;
	virtual String GetNextChatName(String name) = 0;
	virtual String GetPreviousChatName(String name) = 0;

	virtual CowBuffer<ObjectStorage::ID> ListThreads() = 0;
	virtual ObjectStorage::ID GetRootMessageForThread(
		const ObjectStorage::ID &threadID) = 0;
	virtual ObjectStorage::ID GetNextMessage(
		const ObjectStorage::ID &identifier) = 0;
	virtual ObjectStorage::ID GetPreviousMessage(
		const ObjectStorage::ID &identifier) = 0;
	virtual MessageDescriptorBase *GetMessageDescriptor(
		const ObjectStorage::ID &identifier) = 0;
	virtual bool HasUnread(String chatName = String()) = 0;

	virtual void SendMessage(MessageDraft *draft) = 0;
};

// Voice chat.
class VoiceEventProcessor
{
public:
	virtual ~VoiceEventProcessor()
	{ }

	enum VoiceState
	{
		VoiceStateOff = 0,
		VoiceStateInit = 1,
		VoiceStateAsk = 2,
		VoiceStateWait = 3,
		VoiceStateActive = 4
	};

	virtual String GetPeerName() = 0;
	virtual VoiceState GetState() = 0;
	virtual bool IsMuted() = 0;
	virtual bool IsSilent() = 0;

	virtual void ToggleMute() = 0;
};

// Network session.
class NetworkEventProcessor
{
public:
	class ContactListProcessor
	{
	public:
		virtual ~ContactListProcessor()
		{ }

		virtual void ProcessContactList(
			bool success,
			const CommandListContacts::Response &contactList) = 0;
	};

	virtual ~NetworkEventProcessor()
	{ }

	virtual bool ConnectionActive() = 0;
	virtual bool HandshakeActive() = 0;

	virtual void StartConnection(
		int fd,
		const Crypto::X25519::PublicKeyContainer &serverKey) = 0;

	virtual uint64_t GetMaxMessageSize() = 0;

	virtual bool AddContact(String name) = 0;
	virtual bool UpdateContactKey(
		String contactName,
		const Crypto::X25519::PublicKeyContainer &key,
		bool validated,
		bool blocked,
		bool setAsDefault) = 0;
	virtual bool BlockContact(
		String contactName,
		Contact::BlockStatus block) = 0;

	virtual bool ListContacts() = 0;
	virtual bool SetContactListProcessor(
		ContactListProcessor *processor) = 0;

	virtual bool SendMessage(const CowBuffer<uint8_t> message) = 0;
};

// UI.
class UIEventProcessor
{
public:
	virtual ~UIEventProcessor()
	{ }

	virtual void Redraw() = 0;
	virtual void Notify(String message) = 0;
	virtual void *BlockNotify(String message) = 0;
	virtual void BlockCancel(void *handle) = 0;
};

#endif
