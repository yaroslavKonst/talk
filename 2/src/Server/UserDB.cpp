#include "UserDB.hpp"

#include "../ThirdParty/monocypher.h"

UserDB::UserDB() : _userFile("talkd.users", false)
{
	_first = nullptr;
	_last = nullptr;

	LoadUserData();
}

UserDB::~UserDB()
{
	FreeUserData();
}

bool UserDB::HasUser(const uint8_t key[KEY_SIZE])
{
	UserData *data = _first;

	while (data) {
		int res = crypto_verify32(key, data->PublicKey);

		if (!res) {
			return true;
		}

		data = data->Next;
	}

	return false;
}

const uint8_t *UserDB::GetUserPublicKey(const uint8_t key[KEY_SIZE])
{
	UserData *data = _first;

	while (data) {
		int res = crypto_verify32(key, data->PublicKey);

		if (!res) {
			return data->PublicKey;
		}

		data = data->Next;
	}

	THROW("Requested user does not exist.");
}


const uint8_t *UserDB::GetUserSignature(const uint8_t key[KEY_SIZE])
{
	UserData *data = _first;

	while (data) {
		int res = crypto_verify32(key, data->PublicKey);

		if (!res) {
			return data->SignaturePublicKey;
		}

		data = data->Next;
	}

	THROW("Requested user does not exist.");
}

int64_t UserDB::GetUserAccessTime(const uint8_t key[KEY_SIZE])
{
	UserData *data = _first;

	while (data) {
		int res = crypto_verify32(key, data->PublicKey);

		if (!res) {
			return data->AccessTime;
		}

		data = data->Next;
	}

	THROW("Requested user does not exist.");
}

void UserDB::UpdateUserAccessTime(
	const uint8_t key[KEY_SIZE],
	int64_t accessTime)
{
	UserData *data = _first;

	while (data) {
		int res = crypto_verify32(key, data->PublicKey);

		if (!res) {
			data->AccessTime = accessTime;

			_userFile.Write<int64_t>(
				&accessTime,
				1,
				data->IndexInFile * _EntrySize +
				_UserAccessTimeOffset);
		}

		data = data->Next;
	}

	THROW("Requested user does not exist.");
}

void UserDB::AddUser(
	const uint8_t key[KEY_SIZE],
	const uint8_t signature[SIGNATURE_PUBLIC_KEY_SIZE],
	int64_t accessTime)
{
	uint64_t freeIndex = 0;

	UserData *data = _first;

	while (data) {
		if (data->IndexInFile != freeIndex) {
			break;
		}

		++freeIndex;
		data = data->Next;
	}

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

	data = new UserData;
	data->Next = nullptr;
	data->IndexInFile = freeIndex;

	memcpy(data->PublicKey, key, KEY_SIZE);
	memcpy(data->SignaturePublicKey, signature, SIGNATURE_PUBLIC_KEY_SIZE);
	data->AccessTime = accessTime;

	if (!_first) {
		_first = data;
		_last = data;
	} else if (freeIndex < _first->IndexInFile) {
		data->Next = _first;
		_first = data;
	} else {
		UserData *prev = _first;

		while (prev->IndexInFile != freeIndex - 1) {
			prev = prev->Next;
		}

		data->Next = prev->Next;
		prev->Next = data;
	}
}

void UserDB::RemoveUser(const uint8_t key[KEY_SIZE])
{
	UserData **data = &_first;

	while (*data) {
		int res = crypto_verify32(key, (*data)->PublicKey);

		if (!res) {
			break;
		}

		data = &((*data)->Next);
	}

	if (!*data) {
		THROW("Requested user does not exist.");
	}

	uint8_t *buffer = new uint8_t[_EntrySize];
	memset(buffer, 0, _EntrySize);

	_userFile.Write<uint8_t>(
		buffer,
		_EntrySize,
		(*data)->IndexInFile * _EntrySize);

	delete[] buffer;

	UserData *entryToRemove = *data;
	*data = (*data)->Next;
	crypto_wipe(entryToRemove->PublicKey, KEY_SIZE);
	crypto_wipe(
		entryToRemove->SignaturePublicKey,
		SIGNATURE_PUBLIC_KEY_SIZE);
	delete entryToRemove;
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
			continue;
		}

		UserData *newUser = new UserData;
		newUser->Next = nullptr;
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

		if (!_first) {
			_first = newUser;
			_last = newUser;
		} else {
			_last->Next = newUser;
			_last = _last->Next;
		}
	}
}

void UserDB::FreeUserData()
{
	while (_first) {
		UserData *data = _first;
		_first = _first->Next;

		crypto_wipe(data->PublicKey, KEY_SIZE);
		crypto_wipe(
			data->SignaturePublicKey,
			SIGNATURE_PUBLIC_KEY_SIZE);
		delete data;
	}

	_last = nullptr;
}
