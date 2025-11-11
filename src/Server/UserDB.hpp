#ifndef _USERDB_HPP
#define _USERDB_HPP

#include "../Crypto/Crypto.hpp"
#include "../Common/BinaryFile.hpp"

class UserDB
{
public:
	UserDB();
	~UserDB();

	bool HasUser(const uint8_t key[KEY_SIZE]);

	const uint8_t *GetUserPublicKey(const uint8_t key[KEY_SIZE]);
	const uint8_t *GetUserSignature(const uint8_t key[KEY_SIZE]);
	int64_t GetUserAccessTime(const uint8_t key[KEY_SIZE]);
	String GetUserName(const uint8_t key[KEY_SIZE]);

	void UpdateUserAccessTime(
		const uint8_t key[KEY_SIZE],
		int64_t accessTime);

	void AddUser(
		const uint8_t key[KEY_SIZE],
		const uint8_t signature[SIGNATURE_PUBLIC_KEY_SIZE],
		int64_t accessTime,
		String name);

	void RemoveUser(const uint8_t key[KEY_SIZE]);

private:
	struct UserData
	{
		UserData *Next;

		uint64_t IndexInFile;

		uint8_t PublicKey[KEY_SIZE];
		uint8_t SignaturePublicKey[SIGNATURE_PUBLIC_KEY_SIZE];
		int64_t AccessTime;
		String Name;
	};

	// User file structure.
	// File consists of records with equal size.
	// Each record contains data about one user.
	//
	// Record structure.
	// Valid, 8 bit unsigned integer.
	// User key, KEY_SIZE bytes.
	// User signature, SIGNATURE_PUBLIC_KEY_SIZE bytes.
	// Access time, 64 bit signed integer.
	// User name, C-string. 55 bytes are reserved.
	const int64_t _MaxNameLength = 55;

	const uint64_t _EntrySize =
		1 + KEY_SIZE + SIGNATURE_PUBLIC_KEY_SIZE +
		sizeof(int64_t) + _MaxNameLength;
	const uint64_t _ValidOffset = 0;
	const uint64_t _UserKeyOffset = 1;
	const uint64_t _UserSignatureOffset = 1 + KEY_SIZE;
	const uint64_t _UserAccessTimeOffset =
		1 + KEY_SIZE + SIGNATURE_PUBLIC_KEY_SIZE;
	const uint64_t _UserNameOffset =
		1 + KEY_SIZE + SIGNATURE_PUBLIC_KEY_SIZE +
		sizeof(int64_t);

	BinaryFile _userFile;

	UserData *_first;

	void LoadUserData();
	void FreeUserData();
};

#endif
