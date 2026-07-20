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

	_defaultKeyIndex = -1;

	Crypto::X25519::PublicKeyContainer defaultKey;
	bool hasDefaultKey = false;

	if (FileExists(path + "/DefaultKey")) {
		BinaryFile file(path + "/DefaultKey", false);
		file.Read<uint8_t>(defaultKey.Key, Crypto::X25519::KEY_SIZE, 0);
		hasDefaultKey = true;
	}

	CowBuffer<String> keyList = ListDirectory(path);

	int keyCount = keyList.Size();

	if (FileExists(path + "/DefaultKey")) {
		--keyCount;
	}

	if (FileExists(path + "/Blocked")) {
		--keyCount;
	}

	_keys = CowBuffer<Crypto::X25519::PublicKeyContainer>(keyCount);
	_verifiedKeys = CowBuffer<bool>(keyCount);
	_blockedKeys = CowBuffer<bool>(keyCount);

	long currentKeyIndex = 0;

	for (unsigned long keyIdx = 0; keyIdx < keyList.Size(); keyIdx++) {
		Crypto::X25519::PublicKeyContainer key;

		if (keyList[keyIdx] == "DefaultKey") {
			continue;
		}

		if (keyList[keyIdx] == "Blocked") {
			continue;
		}

		if (keyList[keyIdx].Length() != Crypto::X25519::KEY_SIZE * 2) {
			THROW("Invalid key file name.");
		}

		HexToData(keyList[keyIdx], key.Key);

		BinaryFile file(path + "/" + keyList[keyIdx], false);
		uint8_t attrs;
		file.Read<uint8_t>(&attrs, 1, 0);

		_keys[currentKeyIndex] = key;
		_verifiedKeys[currentKeyIndex] = attrs & Verified;
		_blockedKeys[currentKeyIndex] = attrs & Blocked;

		if (hasDefaultKey && key == defaultKey) {
			_defaultKeyIndex = currentKeyIndex;
			hasDefaultKey = false;
		}

		++currentKeyIndex;
	}

	if (FileExists(path + "/Blocked")) {
		BinaryFile file(path + "/Blocked", false);
		file.Read<BlockStatus>(&_blockStatus, 1, 0);
	} else {
		_blockStatus = BlockStatus::Allowed;
	}
}

CowBuffer<Crypto::X25519::PublicKeyContainer> Contact::GetKeys()
{
	return _keys;
}

bool Contact::HasDefaultKey()
{
	return _defaultKeyIndex != -1;
}

Crypto::X25519::PublicKeyContainer Contact::GetDefaultKey()
{
	if (_defaultKeyIndex == -1) {
		THROW("Requested nonexistent default key.");
	}

	return _keys[_defaultKeyIndex];
}

Contact::BlockStatus Contact::GetBlockStatus()
{
	return _blockStatus;
}

bool Contact::IsKeyVerified(const Crypto::X25519::PublicKeyContainer &key)
{
	for (unsigned int i = 0; i < _keys.Size(); i++) {
		if (_keys[i] == key) {
			return _verifiedKeys[i];
		}
	}

	return false;
}

bool Contact::IsKeyBlocked(const Crypto::X25519::PublicKeyContainer &key)
{
	for (unsigned int i = 0; i < _keys.Size(); i++) {
		if (_keys[i] == key) {
			return _blockedKeys[i];
		}
	}

	return false;
}

void Contact::UpdateKey(
	const Crypto::X25519::PublicKeyContainer &key,
	bool verified,
	bool blocked)
{
	String keyHex = DataToHex(key.Key, Crypto::X25519::KEY_SIZE);

	BinaryFile file(_path + "/" + keyHex, true);
	uint8_t attrs = 0;

	if (verified) {
		attrs |= Verified;
	}

	if (blocked) {
		attrs |= Blocked;
	}

	file.Write<uint8_t>(&attrs, 1, 0);

	uint32_t keyIndex = 0;

	while (keyIndex < _keys.Size()) {
		if (key == _keys[keyIndex]) {
			break;
		}

		++keyIndex;
	}

	if (keyIndex >= _keys.Size()) {
		_keys.Resize(_keys.Size() + 1);
		_verifiedKeys.Resize(_keys.Size());
		_blockedKeys.Resize(_keys.Size());

		_keys[_keys.Size() - 1] = key;
	}

	_verifiedKeys[keyIndex] = verified;
	_blockedKeys[keyIndex] = blocked;
}

void Contact::SetDefaultKey(const Crypto::X25519::PublicKeyContainer &key)
{
	BinaryFile file(_path + "/DefaultKey", true);
	file.Write<uint8_t>(key.Key, Crypto::X25519::KEY_SIZE, 0);

	for (unsigned int i = 0; i < _keys.Size(); i++) {
		if (_keys[i] == key) {
			_defaultKeyIndex = i;
			return;
		}
	}

	UpdateKey(key, false, false);
	_defaultKeyIndex = _keys.Size() - 1;
}

void Contact::SetBlockStatus(BlockStatus value)
{
	_blockStatus = value;

	BinaryFile file(_path + "/Blocked", true);
	file.Write<BlockStatus>(&_blockStatus, 1, 0);
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

bool ContactStorage::HasContact(String name)
{
	Tree<ContactNode>::Entry *entry = _contacts.FindEntry(name);
	return entry;
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
	if (HasContact(name)) {
		return;
	}

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
