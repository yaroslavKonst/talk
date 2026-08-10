#ifndef _CONTACT_STORAGE_HPP
#define _CONTACT_STORAGE_HPP

#include "../Common/CowBuffer.hpp"
#include "../Common/Tree.hpp"
#include "../Crypto/Crypto.hpp"

class ContactStorage;

class Contact
{
public:
	enum class BlockStatus : uint8_t
	{
		Allowed = 0,
		Blocked = 1,
		SilentlyBlocked = 2
	};

	Contact(String name, String path);

	CowBuffer<Crypto::X25519::PublicKeyContainer> GetKeys();

	bool HasDefaultKey();
	Crypto::X25519::PublicKeyContainer GetDefaultKey();

	BlockStatus GetBlockStatus();

	bool IsKeyVerified(const Crypto::X25519::PublicKeyContainer &key);
	bool IsKeyBlocked(const Crypto::X25519::PublicKeyContainer &key);

	void UpdateKey(
		const Crypto::X25519::PublicKeyContainer &key,
		bool verified,
		bool blocked);

	void SetDefaultKey(const Crypto::X25519::PublicKeyContainer &key);

	void SetBlockStatus(BlockStatus value);

private:
	enum KeyStatus : uint8_t
	{
		Verified = 0x1,
		Blocked = 0x2
	};

	String _name; // name@host
	CowBuffer<Crypto::X25519::PublicKeyContainer> _keys;
	CowBuffer<bool> _verifiedKeys;
	CowBuffer<bool> _blockedKeys;

	int32_t _defaultKeyIndex;

	BlockStatus _blockStatus;

	String _path;
};

class ContactStorage
{
public:
	ContactStorage(String root);
	~ContactStorage();

	String GetFirstContact();
	CowBuffer<String> GetContactRange(String center, int size);
	bool HasContact(String name);

	String GetNextContact(String name);
	String GetPreviousContact(String name);

	void AddNewContact(String name);
	Contact *GetContact(String name);
	void RemoveContact(String name);

private:
	String _root;

	struct ContactNode
	{
		Contact *Cont;
		String Name;

		bool operator<(const ContactNode &node) const;
		bool operator==(const ContactNode &node) const;

		ContactNode()
		{
			Cont = nullptr;
			Name = "";
		}

		ContactNode(String name)
		{
			Cont = nullptr;
			Name = name;
		}
	};

	Tree<ContactNode> _contacts;

	void LoadContacts();
	void UnloadContacts();
};

#endif
