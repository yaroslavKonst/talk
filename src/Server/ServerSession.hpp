#ifndef _SERVER_SESSION_HPP
#define _SERVER_SESSION_HPP

#include "Config.hpp"
#include "../Protocol/SessionProtocol.hpp"
#include "../Message/ContactStorage.hpp"
#include "../Protocol/SessionParser.hpp"
#include "../Common/EventDispatcher.hpp"
#include "../Common/StreamReader.hpp"
#include "../Common/StreamWriter.hpp"
#include "../Common/ObjectStorage.hpp"
#include "../Crypto/Crypto.hpp"

class ServerSessionStorage;
class StreamProcessorBase;

class ServerSession :
	public DescriptorEventProcessor,
	public TimeEventProcessor,
	public ConfigUser
{
public:
	ServerSession(
		int fd,
		ServerSessionStorage *storage,
		Config* config,
		EventDispatcher *dispatcher,
		StreamProcessorBase *streamProcessor,
		const Crypto::X25519::EncryptedStream &outES,
		const Crypto::X25519::EncryptedStream &inES,
		uint8_t outScramblerInit,
		uint8_t inScramblerInit);
	~ServerSession();

	void ReloadConfig() override;

	int GetDescriptor() override
	{
		return _fd;
	}

	bool RequestRead() override;
	bool RequestWrite() override;

	void ProcessRead() override;
	void ProcessWrite() override;

	void ProcessTimeEvent() override;

	void SendObjects();

	void SendStreamInit(int32_t responseCode);
	void SendStreamEnd();
	void SendStreamRequest(const CowBuffer<uint8_t> initRequest);
	void SendStreamResponse(const CowBuffer<uint8_t> initResponse);

private:
	EventDispatcher *_dispatcher;

	int _fd;
	ServerSessionStorage *_storage;
	StreamProcessorBase *_streamProcessor;
	Config *_config;

	Crypto::X25519::EncryptedStream _inES;
	Crypto::X25519::EncryptedStream _outES;

	SessionProtocol *_protocol;

	bool ProcessInput(const CowBuffer<uint8_t> buffer);

	bool ProcessKeepAlive(const CowBuffer<uint8_t> buffer);
	bool ProcessGetHostName();
	bool ProcessGetMaxMessageSize();
	bool ProcessGetAccountSettings();
	bool ProcessSetAccountSettings(const CowBuffer<uint8_t> buffer);
	bool ProcessAddContact(const CowBuffer<uint8_t> buffer);
	bool ProcessUpdateContactKey(const CowBuffer<uint8_t> buffer);
	bool ProcessBlockContact(const CowBuffer<uint8_t> buffer);
	bool ProcessRemoveContact(const CowBuffer<uint8_t> buffer);
	bool ProcessListContacts();
	bool ProcessSendMessage(const CowBuffer<uint8_t> buffer);
	bool ProcessUpdateMessage(const CowBuffer<uint8_t> buffer);
	bool ProcessOfferMessage(const CowBuffer<uint8_t> buffer);
	bool ProcessStreamInit(const CowBuffer<uint8_t> buffer);
	bool ProcessStreamRequest(const CowBuffer<uint8_t> buffer);
	bool ProcessStreamEnd();

	bool _objectTransmissionActive;
	ObjectStorage::ID _currentObjectID;
	void InitObjectTransmission();
	void ObjectTransmissionStep(const ObjectStorage::ID &id);
	void SendIDRequest();
	void SendID(const ObjectStorage::ID &id);
	bool ProcessRequestID(const CowBuffer<uint8_t> buffer);

	void SendObject(const CowBuffer<uint8_t> object);
	void SendAddContact(const CowBuffer<uint8_t> object);
	void SendUpdateContactKey(const CowBuffer<uint8_t> object);
	void SendBlockContact(const CowBuffer<uint8_t> object);
	void SendRemoveContact(const CowBuffer<uint8_t> object);
	void SendOfferMessage(const CowBuffer<uint8_t> object);
	String _offeredMessagePeerName;
	ObjectStorage::ID _offeredMessageID;
	void SendMessage();
	void SendMessageUpdate(const CowBuffer<uint8_t> object);

	void SessionLog(String message);
};

class ServerSessionStorage
{
public:
	virtual ~ServerSessionStorage()
	{ }

	virtual void MarkSessionForRemoval(ServerSession *session) = 0;

	virtual String GetName() = 0;
	virtual ObjectStorage *GetObjectStorage() = 0;
	virtual void GetAccountSettings(
		bool &messages,
		bool &calls,
		bool &list) = 0;
	virtual void SetAccountSettings(
		bool messages,
		bool calls,
		bool list) = 0;
	virtual void AddContact(String name) = 0;
	virtual void UpdateContactKey(
		String name,
		const Crypto::X25519::PublicKeyContainer &key,
		bool validated,
		bool blocked,
		bool setAsDefault) = 0;
	virtual void BlockContact(
		String name,
		Contact::BlockStatus block) = 0;
	virtual void RemoveContact(String name) = 0;
	virtual CowBuffer<CommandListContacts::Response::UserData>
		GetContactList() = 0;

	virtual bool SendMessage(
		const CowBuffer<uint8_t> message,
		Message::Attribute attr) = 0;
	virtual CowBuffer<uint8_t> GetMessage(
		String peerName,
		const ObjectStorage::ID &messageID,
		Message::Attribute &attr) = 0;
	virtual bool ProcessUpdateMessageRequest(
		String peerName,
		const ObjectStorage::ID &messageID,
		Message::Attribute attr,
		bool value) = 0;
};

class StreamProcessorBase
{
public:
	virtual ~StreamProcessorBase()
	{ }

	virtual void EndStream(ServerSession *session) = 0;
	virtual bool ProcessUserStreamInit(
		const CowBuffer<uint8_t> initRequest,
		ServerSession *session) = 0;
	virtual void NotifyUserSessionClosed(ServerSession *session) = 0;
	virtual bool ProcessUserStreamResponse(
		const CowBuffer<uint8_t> initResponse,
		ServerSession *session) = 0;
	virtual void CheckNewSession(ServerSession *session) = 0;
};

#endif
