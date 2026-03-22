#include "UserDB.hpp"

#include "../Common/File.hpp"

UserDB::UserDB(
	EventDispatcher *dispatcher,
	const uint8_t *privateKey,
	const uint8_t *publicKey)
{
	_dispatcher = dispatcher;

	_privateKey = privateKey;
	_publicKey = publicKey;

	_startupSessions = nullptr;
	_timeQuantRequested = false;

	LoadUserData();
}

UserDB::~UserDB()
{
	if (_timeQuantRequested) {
		_dispatcher->UnregisterQuantProcessor(this);
		_timeQuantRequested = false;
	}

	while (_startupSessions) {
		StartupSession *tmp = _startupSessions;
		_startupSessions = _startupSessions->Next;
		delete tmp->Session;
		delete tmp;
	}

	FreeUserData();
}

bool UserDB::HasUser(String name)
{
	Tree<UserByName>::Entry *data = _usersByName.FindEntry(name);
	return data;
}

bool UserDB::HasUser(const uint8_t key[KEY_SIZE])
{
	Tree<UserByKey>::Entry *data = _usersByKey.FindEntry(key);
	return data;
}

User *UserDB::GetUser(String name)
{
	Tree<UserByName>::Entry *data = _usersByName.FindEntry(name);

	if (!data) {
		return nullptr;
	}

	return data->Key.user;
}

User *UserDB::GetUser(const uint8_t key[KEY_SIZE])
{
	Tree<UserByKey>::Entry *data = _usersByKey.FindEntry(key);

	if (!data) {
		return nullptr;
	}

	return data->Key.user;
}

void UserDB::AddUser(String name, const uint8_t key[KEY_SIZE])
{
	if (HasUser(name)) {
		THROW("User with name " + name + " already exists.");
	}

	if (HasUser(key)) {
		THROW("User with the same key already exists.");
	}

	User::CreateUser(name, key);
}

void UserDB::RemoveUser(String name)
{
	User *user = GetUser(name);

	if (!user) {
		THROW("Requested user does not exist.");
	}

	const uint8_t *key = user->GetPublicKey();

	Tree<UserByName>::Entry *nameEntry = _usersByName.FindEntry(name);
	Tree<UserByKey>::Entry *keyEntry = _usersByKey.FindEntry(key);

	_usersByName.RemoveEntry(nameEntry);
	_usersByKey.RemoveEntry(keyEntry);

	delete user;
	User::RemoveUser(name);
}

int UserDB::GetUserCount()
{
	int userCount = 0;

	Tree<UserByKey>::Entry *entry = _usersByKey.FindSmallest();

	while (entry) {
		++userCount;
		entry = _usersByKey.Next(entry);
	}

	return userCount;
}

CowBuffer<String> UserDB::ListUsers()
{
	int userCount = GetUserCount();
	CowBuffer<String> data(userCount);

	int index = 0;
	Tree<UserByName>::Entry *entry = _usersByName.FindSmallest();

	while (entry) {
		data[index] = entry->Key.user->GetName();
		++index;
		entry = _usersByName.Next(entry);
	}

	return data;
}

void UserDB::AddSession(int fd)
{
	StartupSession *s = new StartupSession;

	s->Next = _startupSessions;
	s->Remove = false;
	s->Session = new ServerHandshake(
		fd,
		this,
		_dispatcher,
		_privateKey,
		_publicKey);

	_startupSessions = s;
}

void UserDB::MarkSessionForRemoval(ServerHandshake *session)
{
	StartupSession *s = _startupSessions;

	while (s) {
		if (s->Session == session) {
			s->Remove = true;
		}

		s = s->Next;
	}

	if (!_timeQuantRequested) {
		_dispatcher->RegisterQuantProcessor(this);
		_timeQuantRequested = true;
	}
}

void UserDB::ProcessQuant()
{
	_timeQuantRequested = false;

	StartupSession **s = &_startupSessions;

	while (*s) {
		if ((*s)->Remove) {
			delete (*s)->Session;

			StartupSession *tmp = *s;
			*s = (*s)->Next;
			delete tmp;
		} else {
			s = &(*s)->Next;
		}
	}
}

UserDB::UserByName::UserByName()
{
	user = nullptr;
}

UserDB::UserByName::UserByName(User *u)
{
	user = u;
	Name = user->GetName();
}

UserDB::UserByName::UserByName(String name)
{
	user = nullptr;
	Name = name;
}

bool UserDB::UserByName::operator<(const UserByName &u) const
{
	return Name < u.Name;
}

bool UserDB::UserByName::operator==(const UserByName &u) const
{
	return Name == u.Name;
}

UserDB::UserByKey::UserByKey()
{
	user = nullptr;
	PublicKey = nullptr;
}

UserDB::UserByKey::UserByKey(User *u)
{
	user = u;
	PublicKey = user->GetPublicKey();
}

UserDB::UserByKey::UserByKey(const uint8_t *publicKey)
{
	user = nullptr;
	PublicKey = publicKey;
}

bool UserDB::UserByKey::operator<(const UserByKey &u) const
{
	for (int i = 0; i < KEY_SIZE; i++) {
		if (PublicKey[i] != u.PublicKey[i]) {
			return PublicKey[i] < u.PublicKey[i];
		}
	}

	return false;
}

bool UserDB::UserByKey::operator==(const UserByKey &u) const
{
	for (int i = 0; i < KEY_SIZE; i++) {
		if (PublicKey[i] != u.PublicKey[i]) {
			return false;
		}
	}

	return true;
}

void UserDB::LoadUserData()
{
	String root = "storage/users";

	if (!FileExists(root)) {
		CreateDirectory("storage");
		CreateDirectory(root);
	}

	CowBuffer<String> userNames = ListDirectory(root);

	for (unsigned int i = 0; i < userNames.Size(); i++) {
		User *user = new User(userNames[i], _dispatcher);

		_usersByName.AddEntry(user);
		_usersByKey.AddEntry(user);
	}
}

void UserDB::FreeUserData()
{
	Tree<UserByName>::Entry *entry = _usersByName.FindSmallest();

	while (entry) {
		delete entry->Key.user;
		entry = _usersByName.Next(entry);
	}
}
