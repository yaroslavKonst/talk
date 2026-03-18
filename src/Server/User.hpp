#ifndef _USER_HPP
#define _USER_HPP

#include "../Common/MyString.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

class User
{
public:
	static void CreateUser(String name, const uint8_t publicKey[KEY_SIZE]);
	static void RemoveUser(String name);

	User(String name);
	~User();

	const uint8_t *GetPublicKey()
	{
		return _publicKey;
	}

	String GetName()
	{
		return _name;
	}

	void AddSession(int fd);
	//void MarkSessionForRemoval(ServerSession *session) override;

private:
	struct UserSession
	{
		UserSession *Next;
		bool Remove;
	};

	UserSession *_sessions;

	String _root;

	String _name;
	uint8_t _publicKey[KEY_SIZE];

	void LoadPublicKey();
	static void StorePublicKey(
		String root,
		const uint8_t publicKey[KEY_SIZE]);
};

#endif
