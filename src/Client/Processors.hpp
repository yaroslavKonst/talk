#ifndef _PROCESSORS_HPP
#define _PROCESSORS_HPP

#include "../Message/ContactStorage.hpp"
#include "../Common/CowBuffer.hpp"
#include "../Common/MyString.hpp"
#include "../Common/ObjectStorage.hpp"
#include "../Crypto/Crypto.hpp"

// Processors are objects that can process events of certain types.

// Text chats.
class MessageEventProcessor
{
public:
	virtual ~MessageEventProcessor()
	{ }

	virtual ContactStorage *GetContactStorage() = 0;
	virtual ObjectStorage::ID GetKnownID() = 0;
	virtual void SetKnownID(const ObjectStorage::ID &id) = 0;

	virtual void SelectOrCreateChat(String peerName) = 0;
	virtual void Activate() = 0;
	virtual void Deactivate() = 0;
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
	virtual ~NetworkEventProcessor()
	{ }

	virtual bool ConnectionActive() = 0;
	virtual bool HandshakeActive() = 0;

	virtual void StartConnection(
		int fd,
		const Crypto::X25519::PublicKeyContainer &serverKey) = 0;

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
