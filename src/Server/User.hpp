#ifndef _USER_HPP
#define _USER_HPP

#include "ServerSession.hpp"
#include "StreamHandler.hpp"
#include "../Protocol/StreamParser.hpp"
#include "../Message/Message.hpp"
#include "../Message/ContactStorage.hpp"
#include "../Common/MyString.hpp"
#include "../Common/ObjectStorage.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

class UserStorage;
class StreamProcessor;

class User :
	public ServerSessionStorage,
	public QuantEventProcessor,
	public ObjectStorageUser,
	public StreamHandlerUser
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

	bool CanBeDeleted();

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

	void GetAccountSettings(
		bool &messages, // True: contacts only.
		bool &calls, // True: contacts only.
		bool &list) override;
	void SetAccountSettings(
		bool messages,
		bool calls,
		bool list) override;

	enum AccountSettingsValues
	{
		SettingForbidMessages = 0x1,
		SettingForbidCalls = 0x2,
		SettingShowInContactList = 0x4
	};

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
	void RemoveContact(String name) override;

	ObjectStorage *GetObjectStorage() override
	{
		return &_objectStorage;
	}

	CowBuffer<CommandListContacts::Response::UserData>
		GetContactList() override;

	// Must be called for outbound messages.
	bool SendMessage(
		const CowBuffer<uint8_t> message,
		Message::Attribute attr) override;

	// Must be called for inbound messages.
	int32_t CheckInboundMessage(
		const Message::X25519::HeaderPointToPoint &header,
		const ObjectStorage::ID &messageID);
	void DeliverMessage(
		const Message::X25519::HeaderPointToPoint &header,
		const CowBuffer<uint8_t> message);

	CowBuffer<uint8_t> GetMessage(
		String peerName,
		const ObjectStorage::ID &messageID,
		Message::Attribute &attr) override;

	// Only for local calls.
	void UpdateMessage(
		String peerName,
		const ObjectStorage::ID &messageID,
		Message::Attribute attr,
		bool value);

	// For update requests from clients. Contains additional checks.
	bool ProcessUpdateMessageRequest(
		String peerName,
		const ObjectStorage::ID &messageID,
		Message::Attribute attr,
		bool value) override;

	StreamHandler *GetStreamHandler();
	void StartStreamGateSession() override;
	int32_t CheckInboundCall(
		const StreamHandshake::InitRequest &request) override;
	bool BroadcastStreamRequest(
		const CowBuffer<uint8_t> initRequest) override;
	void BroadcastStreamEnd(ServerSession *exception) override;

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
		Message::Attribute attr,
		bool inbound);

	StreamHandler _streamHandler;

	struct AccountSettings
	{
		bool AllowMessagesOnlyFromContactList;
		bool AllowCallsOnlyFromContactList;
		bool ShowInContactList;
	};

	AccountSettings _accountSettings;
	void LoadAccountSettings();
	void StoreAccountSettings();
};

class UserStorage
{
public:
	virtual ~UserStorage()
	{ }

	virtual CowBuffer<String> ListUsers(bool publicData) = 0;
	virtual User *GetUser(String name) = 0;

	virtual void RegisterMessageForDelivery(
		const Message::X25519::HeaderPointToPoint &header,
		const ObjectStorage::ID &messageID) = 0;

	virtual void InitStream(StreamHandler *handler) = 0;
};

#endif
