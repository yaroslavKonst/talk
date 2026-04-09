#ifndef _USER_HPP
#define _USER_HPP

#include "ServerSession.hpp"
#include "../Common/MyString.hpp"
#include "../Common/ObjectStorage.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

class User :
	public ServerSessionStorage,
	public QuantEventProcessor,
	public ObjectStorageUser
{
public:
	static void CreateUser(String name, const uint8_t publicKey[KEY_SIZE]);
	static void RemoveUser(String name);

	User(String name, EventDispatcher *dispatcher, Config *config);
	~User();

	const uint8_t *GetPublicKey()
	{
		return _publicKey;
	}

	String GetName()
	{
		return _name;
	}

	void AddSession(
		int fd,
		EncryptedStream *outES,
		EncryptedStream *inES,
		uint8_t outScramblerInit,
		uint8_t inScramblerInit);
	void MarkSessionForRemoval(ServerSession *session) override;

	void ProcessQuant() override;

	void ProcessRequestedObject(
		const ObjectStorage::ID &id,
		const CowBuffer<uint8_t> buffer) override;
	void NotifyWriteCompleted(const ObjectStorage::ID &id) override;

private:
	EventDispatcher *_dispatcher;
	Config *_config;

	ObjectStorage _objectStorage;

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
	uint8_t _publicKey[KEY_SIZE];

	void LoadPublicKey();
	static void StorePublicKey(
		String root,
		const uint8_t publicKey[KEY_SIZE]);
};

#endif
