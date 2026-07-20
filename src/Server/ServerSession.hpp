#ifndef _SERVER_SESSION_HPP
#define _SERVER_SESSION_HPP

#include "Config.hpp"
#include "../Protocol/SessionProtocol.hpp"
#include "../Message/ContactStorage.hpp"
#include "../Common/EventDispatcher.hpp"
#include "../Common/StreamReader.hpp"
#include "../Common/StreamWriter.hpp"
#include "../Common/ObjectStorage.hpp"
#include "../Crypto/Crypto.hpp"

class ServerSessionStorage;

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
		Crypto::X25519::EncryptedStream *outES,
		Crypto::X25519::EncryptedStream *inES,
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

private:
	EventDispatcher *_dispatcher;

	int _fd;
	ServerSessionStorage *_storage;
	Config *_config;

	Crypto::X25519::EncryptedStream _inES;
	Crypto::X25519::EncryptedStream _outES;

	SessionProtocol *_protocol;

	bool ProcessInput(const CowBuffer<uint8_t> buffer);

	bool ProcessKeepAlive(const CowBuffer<uint8_t> buffer);
	bool ProcessGetHostName();
	bool ProcessAddContact(const CowBuffer<uint8_t> buffer);
	bool ProcessUpdateContactKey(const CowBuffer<uint8_t> buffer);
	bool ProcessBlockContact(const CowBuffer<uint8_t> buffer);

	bool _objectTransmissionActive;
	void InitObjectTransmission();
	void ObjectTransmissionStep(const ObjectStorage::ID &id);
	void SendIDRequest();
	void SendID(const ObjectStorage::ID &id);
	bool ProcessRequestID(const CowBuffer<uint8_t> buffer);

	void SendObject(const CowBuffer<uint8_t> object);
	void SendAddContact(const CowBuffer<uint8_t> object);
	void SendUpdateContactKey(const CowBuffer<uint8_t> object);
	void SendBlockContact(const CowBuffer<uint8_t> object);

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
};

#endif
