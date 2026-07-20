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
		Config *config,
		FailBan *failBan,
		const Crypto::X25519::PrivateKeyContainer &privateKey,
		const Crypto::X25519::PublicKeyContainer &publicKey);
	~UserDB();

	bool HasUser(String name);
	User *GetUser(String name);

	void AddUser(
		String name,
		const Crypto::X25519::PublicKeyContainer &key);
	void RemoveUser(String name);

	int GetUserCount();
	CowBuffer<String> ListUsers();

	void AddSession(int fd, int32_t ip);
	void MarkSessionForRemoval(ServerHandshake *session) override;

	void ProcessQuant() override;

private:
	EventDispatcher *_dispatcher;
	Config *_config;
	FailBan *_failBan;

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

	Tree<UserByName> _usersByName;

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

	const Crypto::X25519::PrivateKeyContainer &_privateKey;
	const Crypto::X25519::PublicKeyContainer &_publicKey;
};

#endif
