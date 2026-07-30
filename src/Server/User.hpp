#ifndef _USER_HPP
#define _USER_HPP

#include "ServerSession.hpp"
#include "../Message/Message.hpp"
#include "../Message/ContactStorage.hpp"
#include "../Common/MyString.hpp"
#include "../Common/ObjectStorage.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

class UserStorage;

class User :
	public ServerSessionStorage,
	public QuantEventProcessor,
	public ObjectStorageUser
{
public:
	static void CreateUser(
		String name,
		const Crypto::X25519::PublicKeyContainer &publicKey);
	static void RemoveUser(String name);

	User(
		String name,
		EventDispatcher *dispatcher,
		Config *config,
		UserStorage *userStorage);
	~User();

	const Crypto::X25519::PublicKeyContainer &GetPublicKey()
	{
		return _publicKey;
	}

	String GetName()
	{
		return _name;
	}

	void AddSession(
		int fd,
		const Crypto::X25519::EncryptedStream &outES,
		const Crypto::X25519::EncryptedStream &inES,
		uint8_t outScramblerInit,
		uint8_t inScramblerInit);
	void MarkSessionForRemoval(ServerSession *session) override;

	void ProcessQuant() override;

	void ProcessRequestedObject(
		const ObjectStorage::ID &id,
		const CowBuffer<uint8_t> buffer) override;
	void NotifyWriteCompleted(const ObjectStorage::ID &id) override;

	void AddContact(String name) override;
	void UpdateContactKey(
		String name,
		const Crypto::X25519::PublicKeyContainer &key,
		bool validated,
		bool blocked,
		bool setAsDefault) override;
	void BlockContact(
		String name,
		Contact::BlockStatus block) override;

	ObjectStorage *GetObjectStorage() override
	{
		return &_objectStorage;
	}

	CowBuffer<CommandListContacts::Response::UserData>
		GetContactList() override;

	bool SendMessage(const CowBuffer<uint8_t> message) override;
	CowBuffer<uint8_t> GetMessage(
		String peerName,
		const ObjectStorage::ID &messageID) override;
	void UpdateMessage(
		String peerName,
		const ObjectStorage::ID &messageID,
		Message::Attribute attr,
		bool value);

private:
	EventDispatcher *_dispatcher;
	Config *_config;

	ObjectStorage _objectStorage;
	ContactStorage _contactStorage;
	UserStorage *_userStorage;

	struct UserSession
	{
		UserSession *Next;
		ServerSession *Session;
		bool Remove;
	};

	UserSession *_sessions;
	bool _timeQuantRequested;

	String _root;

	String _name;
	Crypto::X25519::PublicKeyContainer _publicKey;

	void LoadPublicKey();
	static void StorePublicKey(
		String root,
		const Crypto::X25519::PublicKeyContainer &publicKey);

	void AddNewObject(const CowBuffer<uint8_t> object);

	void RegisterNewMessage(
		const Message::X25519::HeaderPointToPoint &header,
		const CowBuffer<uint8_t> message,
		bool inbound);
};

class UserStorage
{
public:
	virtual ~UserStorage()
	{ }

	virtual CowBuffer<String> ListUsers() = 0;
	virtual User *GetUser(String name) = 0;

	virtual void RegisterMessageForDelivery(
		const Message::X25519::HeaderPointToPoint &header,
		const ObjectStorage::ID &messageID) = 0;
};

#endif
