#include "ContactStorage.hpp"

#include "../Common/Hex.hpp"
#include "../Common/BinaryFile.hpp"
#include "../ThirdParty/monocypher.h"

ContactStorage::ContactStorage(const uint8_t *ownerKey)
{
	_ownerKey = ownerKey;

	_contactList = nullptr;
	_contactCount = 0;

	LoadContacts();
}

ContactStorage::~ContactStorage()
{
	FreeContacts();
}

void ContactStorage::AddContact(const uint8_t *peerKey, String name)
{
	String path = String("storage");
	CreateDirectory(path);
	path += String("/") + DataToHex(_ownerKey, KEY_SIZE);
	CreateDirectory(path);
	path += "/contacts";
	CreateDirectory(path);
	path += String("/") + DataToHex(peerKey, KEY_SIZE);

	if (FileExists(path)) {
		UpdateContact(peerKey, name);
		return;
	}

	Contact **list = new Contact*[_contactCount + 1];

	for (int i = 0; i < _contactCount; i++) {
		list[i] = _contactList[i];
	}

	++_contactCount;

	list[_contactCount - 1] = new Contact;
	memcpy(list[_contactCount - 1]->Key, peerKey, KEY_SIZE);
	list[_contactCount - 1]->Name = name;

	if (_contactList) {
		delete[] _contactList;
	}

	_contactList = list;

	BinaryFile file(path, true);
	file.Write<char>(name.CStr(), name.Length(), 0);
}

void ContactStorage::UpdateContact(const uint8_t *peerKey, String name)
{
	int contactIndex;

	for (contactIndex = 0; contactIndex < _contactCount; contactIndex++) {
		bool match = !crypto_verify32(
			peerKey,
			_contactList[contactIndex]->Key);

		if (match) {
			break;
		}
	}

	if (contactIndex == _contactCount) {
		THROW("Tried to update not existing user.");
	}

	_contactList[contactIndex]->Name = name;

	String path = String("storage/") + DataToHex(_ownerKey, KEY_SIZE) +
		"/contacts/" + DataToHex(peerKey, KEY_SIZE);

	BinaryFile file(path, false);
	file.Clear();
	file.Write<char>(name.CStr(), name.Length(), 0);
}

int ContactStorage::GetContactCount()
{
	return _contactCount;
}

const uint8_t *ContactStorage::GetContactKey(int index)
{
	return _contactList[index]->Key;
}

String ContactStorage::GetName(int index)
{
	return _contactList[index]->Name;
}

String ContactStorage::GetNameForPresentation(int index)
{
	if (_contactList[index]->Name.Length() > 0) {
		return _contactList[index]->Name;
	}

	return DataToHex(_contactList[index]->Key, KEY_SIZE);
}

void ContactStorage::LoadContacts()
{
	String path = String("storage/") + DataToHex(_ownerKey, KEY_SIZE) +
		"/contacts";

	if (!FileExists(path)) {
		return;
	}

	CowBuffer<String> entries = ListDirectory(path);

	_contactCount = entries.Size();
	_contactList = new Contact*[_contactCount];

	for (int i = 0; i < _contactCount; i++) {
		_contactList[i] = new Contact;

		HexToData(entries[i], _contactList[i]->Key);

		BinaryFile file(path + "/" + entries[i], false);

		if (file.Size() > 0) {
			char *nameBuffer = new char[file.Size() + 1];
			file.Read<char>(nameBuffer, file.Size(), 0);
			nameBuffer[file.Size()] = 0;
			_contactList[i]->Name = nameBuffer;
			delete[] nameBuffer;
		}
	}
}

void ContactStorage::FreeContacts()
{
	if (!_contactList) {
		return;
	}

	for (int i = 0; i < _contactCount; i++) {
		delete _contactList[i];
	}

	delete[] _contactList;
	_contactList = 0;
}
