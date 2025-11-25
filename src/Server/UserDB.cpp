#include "UserDB.hpp"

#include <cstring>

#include "../ThirdParty/monocypher.h"
#include "../Common/Debug.hpp"

UserDB::UserDB() : _userFile("talkd.users", true)
{
	_users = nullptr;
	_freeIndices = nullptr;
	_deletedUsers = nullptr;

	LoadUserData();
}

UserDB::~UserDB()
{
	FreeUserData();
}

bool UserDB::HasUser(const uint8_t key[KEY_SIZE])
{
	UserTree **data = FindEntry(key);
	return data;
}

const uint8_t *UserDB::GetUserPublicKey(const uint8_t key[KEY_SIZE])
{
	UserTree **data = FindEntry(key);

	if (!data) {
		THROW("Requested user does not exist.");
	}

	return (*data)->Data->PublicKey;
}

const uint8_t *UserDB::GetUserSignature(const uint8_t key[KEY_SIZE])
{
	UserTree **data = FindEntry(key);

	if (!data) {
		THROW("Requested user does not exist.");
	}

	return (*data)->Data->SignaturePublicKey;
}

int64_t UserDB::GetUserAccessTime(const uint8_t key[KEY_SIZE])
{
	UserTree **data = FindEntry(key);

	if (!data) {
		THROW("Requested user does not exist.");
	}

	return (*data)->Data->AccessTime;
}

String UserDB::GetUserName(const uint8_t key[KEY_SIZE])
{
	UserTree **data = FindEntry(key);

	if (!data) {
		THROW("Requested user does not exist.");
	}

	return (*data)->Data->Name;
}

void UserDB::UpdateUserAccessTime(
	const uint8_t key[KEY_SIZE],
	int64_t accessTime)
{
	UserTree **data = FindEntry(key);

	if (!data) {
		THROW("Requested user does not exist.");
	}

	(*data)->Data->AccessTime = accessTime;

	_userFile.Write<int64_t>(
		&accessTime,
		1,
		(*data)->Data->IndexInFile * _EntrySize +
		_UserAccessTimeOffset);
}

void UserDB::AddUser(
	const uint8_t key[KEY_SIZE],
	const uint8_t signature[SIGNATURE_PUBLIC_KEY_SIZE],
	int64_t accessTime,
	String name)
{
	// Free index lookup.
	uint64_t freeIndex;

	if (_freeIndices) {
		freeIndex = _freeIndices->Index;
		FreeIndex *tmp = _freeIndices;
		_freeIndices = _freeIndices->Next;
		delete tmp;
	} else {
		freeIndex = _userFile.Size() / _EntrySize;
	}

	// Write to file.
	char *zeroBuffer = new char[_EntrySize];
	memset(zeroBuffer, 0, _EntrySize);

	_userFile.Write<char>(
		zeroBuffer,
		_EntrySize,
		freeIndex * _EntrySize);

	delete[] zeroBuffer;

	_userFile.Write<uint8_t>(
		key,
		KEY_SIZE,
		freeIndex * _EntrySize + _UserKeyOffset);

	_userFile.Write<uint8_t>(
		signature,
		SIGNATURE_PUBLIC_KEY_SIZE,
		freeIndex * _EntrySize + _UserSignatureOffset);

	_userFile.Write<int64_t>(
		&accessTime,
		1,
		freeIndex * _EntrySize + _UserAccessTimeOffset);

	uint8_t valid = 1;

	_userFile.Write<uint8_t>(
		&valid,
		1,
		freeIndex * _EntrySize + _ValidOffset);

	if (name.Length() >= _MaxNameLength) {
		name = name.Substring(0, _MaxNameLength - 1);
	}

	_userFile.Write<char>(
		name.CStr(),
		name.Length() + 1,
		freeIndex * _EntrySize + _UserNameOffset);

	// Add tree entry.
	UserData *data = new UserData;
	data->IndexInFile = freeIndex;

	memcpy(data->PublicKey, key, KEY_SIZE);
	memcpy(data->SignaturePublicKey, signature, SIGNATURE_PUBLIC_KEY_SIZE);
	data->AccessTime = accessTime;
	data->Name = name;

	UserTree *entry = new UserTree;
	entry->Data = data;
	entry->Left = nullptr;
	entry->Right = nullptr;

	AddEntry(&_users, entry);
}

void UserDB::RemoveUser(const uint8_t key[KEY_SIZE])
{
	// Tree node lookup.
	UserTree **root = FindEntry(key);

	if (!root) {
		THROW("Requested user does not exist.");
	}

	// Erase entry in file.
	uint8_t *buffer = new uint8_t[_EntrySize];
	memset(buffer, 0, _EntrySize);

	_userFile.Write<uint8_t>(
		buffer,
		_EntrySize,
		(*root)->Data->IndexInFile * _EntrySize);

	delete[] buffer;

	// Add free index to list.
	FreeIndex *idx = new FreeIndex;
	idx->Index = (*root)->Data->IndexInFile;
	idx->Next = _freeIndices;
	_freeIndices = idx;

	// Remove tree node.
	UserTree *deletedNode = new UserTree;
	deletedNode->Data = (*root)->Data;
	deletedNode->Left = nullptr;
	deletedNode->Right = _deletedUsers;
	_deletedUsers = deletedNode;

	(*root)->Data = nullptr;
	RemoveEntry(root);
}

int32_t UserDB::GetUserCount()
{
	return GetEntryNumber(_users);
}

CowBuffer<const uint8_t*> UserDB::ListUsers()
{
	int userCount = GetUserCount();

	CowBuffer<const uint8_t*> data(userCount);

	userCount = 0;
	FillUserList(_users, data.Pointer(), &userCount);

	return data;
}

UserDB::UserData::~UserData()
{
	crypto_wipe(PublicKey, KEY_SIZE);
	crypto_wipe(SignaturePublicKey, SIGNATURE_PUBLIC_KEY_SIZE);
}

int UserDB::UserData::Compare(const uint8_t *key)
{
	for (int i = 0; i < KEY_SIZE; i++) {
		if (PublicKey[i] != key[i]) {
			return (int)PublicKey[i] - (int)key[i];
		}
	}

	return 0;
}

UserDB::UserTree::~UserTree()
{
	if (Data) {
		delete Data;
	}

	if (Left) {
		delete Left;
	}

	if (Right) {
		delete Right;
	}
}

UserDB::UserTree **UserDB::FindEntry(const uint8_t *key)
{
	UserTree **root = &_users;

	while (*root) {
		int cmp = (*root)->Data->Compare(key);

		if (!cmp) {
			break;
		}

		if (cmp < 0) {
			root = &((*root)->Right);
		} else {
			root = &((*root)->Left);
		}
	}

	if (!*root) {
		return nullptr;
	}

	return root;
}

void UserDB::AddEntry(UserTree **root, UserTree *entry)
{
	const uint8_t *key = entry->Data->PublicKey;

	while (*root) {
		int cmp = (*root)->Data->Compare(key);

		if (!cmp) {
			THROW("Trying to add duplicate tree node.");
		}

		if (cmp < 0) {
			root = &((*root)->Right);
		} else {
			root = &((*root)->Left);
		}
	}

	*root = entry;
}

void UserDB::RemoveEntry(UserTree **entry)
{
	UserTree *root = *entry;

	if (!root->Left) {
		*entry = root->Right;
		root->Right = nullptr;
		delete root;
		return;
	}

	if (!root->Right) {
		*entry = root->Left;
		root->Left = nullptr;
		delete root;
		return;
	}

	UserTree **leftMax = &(root->Left);

	while ((*leftMax)->Right) {
		leftMax = &((*leftMax)->Right);
	}

	root->Data = (*leftMax)->Data;
	(*leftMax)->Data = nullptr;

	RemoveEntry(leftMax);
}

void UserDB::LoadUserData()
{
	uint64_t entryCount = _userFile.Size() / _EntrySize;

	for (uint64_t entryIdx = 0; entryIdx < entryCount; entryIdx++) {
		uint8_t valid;

		_userFile.Read<uint8_t>(
			&valid,
			1,
			entryIdx * _EntrySize + _ValidOffset);

		if (!valid) {
			FreeIndex *idx = new FreeIndex;
			idx->Index = entryIdx;
			idx->Next = _freeIndices;
			_freeIndices = idx;
			continue;
		}

		UserData *newUser = new UserData;
		newUser->IndexInFile = entryIdx;

		_userFile.Read<uint8_t>(
			newUser->PublicKey,
			KEY_SIZE,
			entryIdx * _EntrySize + _UserKeyOffset);

		_userFile.Read<uint8_t>(
			newUser->SignaturePublicKey,
			SIGNATURE_PUBLIC_KEY_SIZE,
			entryIdx * _EntrySize + _UserSignatureOffset);

		_userFile.Read<int64_t>(
			&newUser->AccessTime,
			1,
			entryIdx * _EntrySize + _UserAccessTimeOffset);

		char *nameBuffer = new char[_MaxNameLength];
		_userFile.Read<char>(
			nameBuffer,
			_MaxNameLength,
			entryIdx * _EntrySize + _UserNameOffset);

		newUser->Name = String(nameBuffer);
		delete[] nameBuffer;

		UserTree *entry = new UserTree;
		entry->Data = newUser;
		entry->Left = nullptr;
		entry->Right = nullptr;

		AddEntry(&_users, entry);
	}
}

void UserDB::FreeUserData()
{
	delete _deletedUsers;
	delete _users;

	while (_freeIndices) {
		FreeIndex *index = _freeIndices;
		_freeIndices = _freeIndices->Next;
		delete index;
	}
}

int32_t UserDB::GetEntryNumber(UserTree *entry)
{
	if (!entry) {
		return 0;
	}

	return 1 + GetEntryNumber(entry->Left) + GetEntryNumber(entry->Right);
}

void UserDB::FillUserList(UserTree *entry, const uint8_t **data, int *index)
{
	if (!entry) {
		return;
	}

	FillUserList(entry->Left, data, index);
	data[*index] = entry->Data->PublicKey;
	*index += 1;
	FillUserList(entry->Right, data, index);
}
