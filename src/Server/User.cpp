#include "User.hpp"

#include <cstring>

#include "../Common/File.hpp"
#include "../Common/BinaryFile.hpp"
#include "../Common/Exception.hpp"
#include "../ThirdParty/monocypher.h"

void User::CreateUser(String name, const uint8_t publicKey[KEY_SIZE])
{
	String root = "storage/users/" + name;

	if (FileExists(root)) {
		THROW("User " + name + " already exists.");
	}

	StorePublicKey(root, publicKey);
}

void User::RemoveUser(String name)
{
	String root = "storage/users/" + name;
	THROW("Not implemented.");
}

User::User(String name, EventDispatcher *dispatcher)
{
	_dispatcher = dispatcher;

	_root = "storage/users/" + name;

	if (!FileExists(_root)) {
		THROW("User " + name + " does not exist.");
	}

	_name = name;
	_sessions = nullptr;
	_timeQuantRequested = false;

	LoadPublicKey();
}

User::~User()
{
	if (_timeQuantRequested) {
		_dispatcher->UnregisterQuantProcessor(this);
		_timeQuantRequested = false;
	}

	while (_sessions) {
		UserSession *session = _sessions;
		_sessions = _sessions->Next;

		delete session->Session;
		delete session;
	}

	crypto_wipe(_publicKey, KEY_SIZE);
}

void User::AddSession(
	int fd,
	EncryptedStream *outES,
	EncryptedStream *inES,
	uint8_t outScramblerInit,
	uint8_t inScramblerInit)
{
	UserSession *s = new UserSession;
	s->Next = _sessions;
	s->Remove = false;
	s->Session = new ServerSession(
		fd,
		this,
		_dispatcher,
		outES,
		inES,
		outScramblerInit,
		inScramblerInit);

	_sessions = s;
}

void User::MarkSessionForRemoval(ServerSession *session)
{
	UserSession *s = _sessions;

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

void User::ProcessQuant()
{
	UserSession **s = &_sessions;

	while (*s) {
		if ((*s)->Remove) {
			UserSession *tmp = *s;
			*s = (*s)->Next;

			delete tmp->Session;
			delete tmp;
		} else {
			s = &(*s)->Next;
		}
	}

	_timeQuantRequested = false;
}

void User::LoadPublicKey()
{
	BinaryFile file(_root + "/key", false);

	if (file.Size() != KEY_SIZE) {
		THROW("Invalid public key size for user " + _name + ".");
	}

	file.Read(_publicKey, KEY_SIZE, 0);
}

void User::StorePublicKey(
	String root,
	const uint8_t publicKey[KEY_SIZE])
{
	BinaryFile file(root + "/key", true);
	file.Write(publicKey, KEY_SIZE, 0);
}
