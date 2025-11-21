#ifndef _CONTACT_STORAGE_HPP
#define _CONTACT_STORAGE_HPP

#include <cstdint>

#include "../Common/MyString.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

class ContactStorage
{
public:
	ContactStorage(const uint8_t *ownerKey);
	~ContactStorage();

	void AddContact(const uint8_t *peerKey, String name);
	void UpdateContact(const uint8_t *peerKey, String name);

	int GetContactCount();

	const uint8_t *GetContactKey(int index);

	String GetName(int index);
	String GetNameForPresentation(int index);

private:
	const uint8_t *_ownerKey;

	struct Contact
	{
		uint8_t Key[KEY_SIZE];
		String Name;
	};

	Contact **_contactList;
	int _contactCount;

	void LoadContacts();
	void FreeContacts();
};

#endif
