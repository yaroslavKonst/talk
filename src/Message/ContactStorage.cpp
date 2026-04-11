#include "ContactStorage.hpp"

#include "../Common/File.hpp"
#include "../Common/BinaryFile.hpp"
#include "../Common/Hex.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

Contact::Contact(String name, String path)
{
	_name = name;
	_path = path;

	if (!FileExists(path)) {
		CreateDirectory(path);
	}

	CowBuffer<String> keyList = ListDirectory(path);
	_keys = CowBuffer<CowBuffer<uint8_t>>(keyList.Size());

	for (unsigned long keyIdx = 0; keyIdx < keyList.Size(); keyIdx++) {
		CowBuffer<uint8_t> key(KEY_SIZE);
		HexToData(keyList[keyIdx], key.Pointer());

		BinaryFile file(path + "/" + keyList[keyIdx], false);
		uint8_t verified;
		file.Read<uint8_t>(&verified, 1, 0);

		_keys[keyIdx] = key;
		_verifiedKeys[keyIdx] = verified;
	}
}

ContactStorage::ContactStorage(String root)
{
	_root = root + "/contacts";

	if (!FileExists(_root)) {
		CreateDirectory(_root);
	}

	LoadContacts();
}

String ContactStorage::GetFirstContact()
{
	Tree<ContactNode>::Entry *entry = _contacts.FindSmallest();

	if (!entry) {
		return "";
	}

	return entry->Key.Name;
}

CowBuffer<String> ContactStorage::GetContactRange(String center, int size)
{
	Tree<ContactNode>::Entry *low = _contacts.FindEntry(center);

	if (!low) {
		low = _contacts.FindSmallest();

		if (!low) {
			return CowBuffer<String>();
		}
	}

	Tree<ContactNode>::Entry *high = low;

	int s = 1;

	bool goUp = false;
	bool prevUpSuccess = true;
	bool prevDownSuccess = true;

	while (s < size) {
		Tree<ContactNode>::Entry *e;

		if (goUp) {
			e = _contacts.Previous(low);

			if (e) {
				low = e;
				++s;
				prevUpSuccess = true;
			} else {
				prevUpSuccess = false;

				if (!prevDownSuccess) {
					break;
				}
			}
		} else {
			e = _contacts.Next(high);

			if (e) {
				high = e;
				++s;
				prevDownSuccess = true;
			} else {
				prevDownSuccess = false;

				if (!prevUpSuccess) {
					break;
				}
			}
		}

		goUp = !goUp;
	}

	CowBuffer<String> result(s);

	while (s > 0) {
		--s;
		result[s] = high->Key.Name;
		high = _contacts.Previous(high);
	}

	return result;
}

String ContactStorage::GetNextContact(String name)
{
	Tree<ContactNode>::Entry *entry = _contacts.FindEntry(name);

	if (!entry) {
		return "";
	}

	entry = _contacts.Next(entry);

	if (!entry) {
		return "";
	}

	return entry->Key.Name;
}

String ContactStorage::GetPreviousContact(String name)
{
	Tree<ContactNode>::Entry *entry = _contacts.FindEntry(name);

	if (!entry) {
		return "";
	}

	entry = _contacts.Previous(entry);

	if (!entry) {
		return "";
	}

	return entry->Key.Name;
}

void ContactStorage::AddNewContact(String name)
{
	Contact *contact = new Contact(
		name,
		_root + "/" + name);

	ContactNode node;
	node.Cont = contact;
	node.Name = name;
	_contacts.AddEntry(node);
}

Contact *ContactStorage::GetContact(String name)
{
	Tree<ContactNode>::Entry *entry = _contacts.FindEntry(name);

	if (!entry) {
		return nullptr;
	}

	return entry->Key.Cont;
}

bool ContactStorage::ContactNode::operator<(const ContactNode &node) const
{
	return Name < node.Name;
}

bool ContactStorage::ContactNode::operator==(const ContactNode &node) const
{
	return Name == node.Name;
}

void ContactStorage::LoadContacts()
{
	CowBuffer<String> contactList = ListDirectory(_root);

	for (unsigned long conIdx = 0; conIdx < contactList.Size(); conIdx++) {
		Contact *contact = new Contact(
			contactList[conIdx],
			_root + "/" + contactList[conIdx]);

		ContactNode node;
		node.Cont = contact;
		node.Name = contactList[conIdx];
		_contacts.AddEntry(node);
	}
}

void ContactStorage::UnloadContacts()
{
	Tree<ContactNode>::Entry *entry = _contacts.FindSmallest();

	while (entry) {
		delete entry->Key.Cont;

		Tree<ContactNode>::Entry *next = _contacts.Next(entry);
		_contacts.RemoveEntry(entry);
		entry = next;
	}
}
