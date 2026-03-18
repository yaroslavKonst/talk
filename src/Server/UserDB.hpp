#ifndef _USERDB_HPP
#define _USERDB_HPP

#include "User.hpp"
#include "ServerHandshake.hpp"
#include "../Crypto/CryptoDefinitions.hpp"
#include "../Common/Tree.hpp"

class UserDB :
	public ServerHandshakeStorage,
	public QuantEventProcessor
{
public:
	UserDB(
		EventDispatcher *dispatcher,
		const uint8_t *privateKey,
		const uint8_t *publicKey);
	~UserDB();

	bool HasUser(String name);
	bool HasUser(const uint8_t key[KEY_SIZE]) override;

	User *GetUser(String name);
	User *GetUser(const uint8_t key[KEY_SIZE]) override;

	void AddUser(String name, const uint8_t key[KEY_SIZE]);
	void RemoveUser(String name);

	int GetUserCount();
	CowBuffer<String> ListUsers();

	void AddSession(int fd);
	void MarkSessionForRemoval(ServerHandshake *session) override;

	void ProcessQuant() override;

private:
	EventDispatcher *_dispatcher;

	struct UserByName
	{
		User *user;
		String Name;

		UserByName();
		UserByName(User *u);
		UserByName(String name);

		bool operator<(const UserByName &u) const;
		bool operator==(const UserByName &u) const;
	};

	struct UserByKey
	{
		User *user;
		const uint8_t *PublicKey;

		UserByKey();
		UserByKey(User *u);
		UserByKey(const uint8_t *publicKey);

		bool operator<(const UserByKey &u) const;
		bool operator==(const UserByKey &u) const;
	};

	Tree<UserByName> _usersByName;
	Tree<UserByKey> _usersByKey;

	void LoadUserData();
	void FreeUserData();

	struct StartupSession
	{
		StartupSession *Next;
		ServerHandshake *Session;
		bool Remove;
	};

	StartupSession *_startupSessions;
	bool _timeQuantRequested;

	const uint8_t *_privateKey;
	const uint8_t *_publicKey;
};

#endif
