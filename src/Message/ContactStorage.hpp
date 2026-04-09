#ifndef _CONTACT_STORAGE_HPP
#define _CONTACT_STORAGE_HPP

#include "../Common/CowBuffer.hpp"
#include "../Common/Tree.hpp"

class Contact
{
public:
	Contact(String name, String path);

private:
	String _name; // name@host
	CowBuffer<CowBuffer<uint8_t>> _keys;
	CowBuffer<bool> _verifiedKeys;

	String _path;
};

class ContactStorage
{
public:
	ContactStorage(String root);

	String GetFirstContact();
	CowBuffer<String> GetContactRange(String center, int size);

	String GetNextContact(String name);
	String GetPreviousContact(String name);

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
