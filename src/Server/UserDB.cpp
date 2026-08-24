#include "UserDB.hpp"

#include "../Common/File.hpp"

UserDB::UserDB(
	EventDispatcher *dispatcher,
	Config *config,
	FailBan *failBan,
	const Crypto::X25519::PrivateKeyContainer &privateKey,
	const Crypto::X25519::PublicKeyContainer &publicKey) :
	_privateKey(privateKey),
	_publicKey(publicKey)
{
	_dispatcher = dispatcher;
	_config = config;
	_failBan = failBan;
	_sendPlanner = nullptr;

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

void UserDB::SetSendPlanner(SendPlannerBase *sendPlanner)
{
	_sendPlanner = sendPlanner;
}

bool UserDB::HasUser(String name)
{
	Tree<UserByName>::Entry *data = _usersByName.FindEntry(name);
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

void UserDB::AddUser(
	String name,
	const Crypto::X25519::PublicKeyContainer &key)
{
	if (HasUser(name)) {
		THROW("User with name " + name + " already exists.");
	}

	User::CreateUser(name, key);

	User *user = new User(name, _dispatcher, _config, this);
	_usersByName.AddEntry(user);
}

bool UserDB::RemoveUser(String name)
{
	Tree<UserByName>::Entry *nameEntry = _usersByName.FindEntry(name);

	if (!nameEntry) {
		THROW("Requested user does not exist.");
	}

	User *user = nameEntry->Key.user;

	if (!user->CanBeDeleted()) {
		return false;
	}

	_usersByName.RemoveEntry(nameEntry);

	delete user;
	User::RemoveUser(name);
	return true;
}

int UserDB::GetUserCount()
{
	int userCount = 0;

	Tree<UserByName>::Entry *entry = _usersByName.FindSmallest();

	while (entry) {
		++userCount;
		entry = _usersByName.Next(entry);
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

void UserDB::AddSession(int fd, IPAddress ip)
{
	StartupSession *s = new StartupSession;

	s->Next = _startupSessions;
	s->Remove = false;
	s->Session = new ServerHandshake(
		fd,
		ip,
		this,
		_dispatcher,
		_failBan,
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

void UserDB::RegisterMessageForDelivery(
	const Message::X25519::HeaderPointToPoint &header,
	const ObjectStorage::ID &messageID)
{
	if (!_sendPlanner) {
		THROW("Send planner is not registered.");
	}

	_sendPlanner->RegisterMessageForDelivery(header, messageID);
}

void UserDB::InitStream(StreamHandler *handler)
{
	_sendPlanner->InitStream(handler);
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

void UserDB::LoadUserData()
{
	String root = "storage/users";

	if (!FileExists(root)) {
		CreateDirectory("storage");
		CreateDirectory(root);
	}

	CowBuffer<String> userNames = ListDirectory(root);

	for (unsigned int i = 0; i < userNames.Size(); i++) {
		User *user = new User(
			userNames[i],
			_dispatcher,
			_config,
			this);
		_usersByName.AddEntry(user);
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
