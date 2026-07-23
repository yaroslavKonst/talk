#include "User.hpp"

#include <cstring>

#include "ObjectType.hpp"
#include "../Common/File.hpp"
#include "../Common/BinaryFile.hpp"
#include "../Common/Exception.hpp"
#include "../Protocol/ParserHelpers.hpp"
#include "../ThirdParty/monocypher.h"

void User::CreateUser(
	String name,
	const Crypto::X25519::PublicKeyContainer &publicKey)
{
	String root = "storage/users/" + name;

	if (FileExists(root)) {
		THROW("User " + name + " already exists.");
	}

	CreateDirectory(root);

	StorePublicKey(root, publicKey);
}

void User::RemoveUser(String name)
{
	String root = "storage/users/" + name;
	THROW("Not implemented.");
}

User::User(String name, EventDispatcher *dispatcher, Config *config) :
	_objectStorage("storage/users/" + name + "/storage", dispatcher),
	_contactStorage("storage/users/" + name)
{
	_dispatcher = dispatcher;
	_config = config;

	_root = "storage/users/" + name;

	if (!FileExists(_root)) {
		THROW("User " + name + " does not exist.");
	}

	_name = name;
	_sessions = nullptr;
	_timeQuantRequested = false;

	LoadPublicKey();

	_objectStorage.SetUser(this);
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
}

void User::AddSession(
	int fd,
	const Crypto::X25519::EncryptedStream &outES,
	const Crypto::X25519::EncryptedStream &inES,
	uint8_t outScramblerInit,
	uint8_t inScramblerInit)
{
	UserSession *s = new UserSession;
	s->Next = _sessions;
	s->Remove = false;
	s->Session = new ServerSession(
		fd,
		this,
		_config,
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

void User::ProcessRequestedObject(
	const ObjectStorage::ID &id,
	const CowBuffer<uint8_t> buffer)
{
}

void User::NotifyWriteCompleted(const ObjectStorage::ID &id)
{
}

void User::AddContact(String name)
{
	_contactStorage.AddNewContact(name);

	NewContactObject::Data data;
	data.ContactName = name;

	AddNewObject(NewContactObject::BuildData(data));
}

void User::UpdateContactKey(
	String name,
	const Crypto::X25519::PublicKeyContainer &key,
	bool validated,
	bool blocked,
	bool setAsDefault)
{
	_contactStorage.GetContact(name)->UpdateKey(key, validated, blocked);

	if (setAsDefault) {
		_contactStorage.GetContact(name)->SetDefaultKey(key);
	}

	UpdateContactKeyObject::Data data;
	data.ContactName = name;
	data.Key = key;
	data.Validated = validated;
	data.Blocked = blocked;
	data.SetAsDefault =setAsDefault;

	AddNewObject(UpdateContactKeyObject::BuildData(data));
}

void User::BlockContact(String name, Contact::BlockStatus block)
{
	_contactStorage.GetContact(name)->SetBlockStatus(block);

	BlockContactObject::Data data;
	data.ContactName = name;
	data.BlockStatus = (uint8_t)block;

	AddNewObject(BlockContactObject::BuildData(data));
}

void User::LoadPublicKey()
{
	BinaryFile file(_root + "/key", false);

	if (file.Size() != Crypto::X25519::KEY_SIZE) {
		THROW("Invalid public key size for user " + _name + ".");
	}

	file.Read<uint8_t>(_publicKey.Key, Crypto::X25519::KEY_SIZE, 0);
}

void User::StorePublicKey(
	String root,
	const Crypto::X25519::PublicKeyContainer &publicKey)
{
	BinaryFile file(root + "/key", true);
	file.Write<uint8_t>(publicKey.Key, Crypto::X25519::KEY_SIZE, 0);
}

void User::AddNewObject(const CowBuffer<uint8_t> object)
{
	ObjectStorage::ID itemId = _objectStorage.GetFreeID(object);

	_objectStorage.WriteObject(itemId, object);

	if (_objectStorage.HasRef(HEAD_REF)) {
		ObjectStorage::ID prevItem = _objectStorage.GetRef(HEAD_REF);

		CowBuffer<uint8_t> idValueBuffer(
			(int)ObjectStorage::Constants::IDSize);

		memcpy(
			idValueBuffer.Pointer(),
			itemId.GetValue(),
			idValueBuffer.Size());

		_objectStorage.UpdateObject(
			prevItem,
			idValueBuffer,
			sizeof(int32_t));
	} else {
		_objectStorage.SetRef(ROOT_REF, itemId);
	}

	_objectStorage.SetRef(HEAD_REF, itemId);

	UserSession *s = _sessions;

	while (s) {
		s->Session->SendObjects();
		s = s->Next;
	}
}
