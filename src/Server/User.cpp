#include "User.hpp"

#include <cstring>

#include "ObjectType.hpp"
#include "../Common/File.hpp"
#include "../Common/BinaryFile.hpp"
#include "../Common/Exception.hpp"
#include "../Common/Log.hpp"
#include "../Protocol/ParserHelpers.hpp"
#include "../Protocol/GateParser.hpp"
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

User::User(
	String name,
	EventDispatcher *dispatcher,
	Config *config,
	UserStorage *userStorage) :
	_objectStorage("storage/users/" + name + "/sequence", dispatcher),
	_contactStorage("storage/users/" + name)
{
	_dispatcher = dispatcher;
	_config = config;
	_userStorage = userStorage;

	_root = "storage/users/" + name;

	if (!FileExists(_root)) {
		THROW("User " + name + " does not exist.");
	}

	if (!FileExists(_root + "/storage")) {
		CreateDirectory(_root + "/storage");
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

CowBuffer<CommandListContacts::Response::UserData> User::GetContactList()
{
	CowBuffer<String> userNames = _userStorage->ListUsers();

	CowBuffer<CommandListContacts::Response::UserData> result(
		userNames.Size());

	for (uint32_t i = 0; i < userNames.Size(); i++) {
		User *user = _userStorage->GetUser(userNames[i]);

		result[i].Name = userNames[i] + "@" + _config->GetHostName();
		result[i].Key = user->GetPublicKey();
	}

	return result;
}

bool User::SendMessage(
	const CowBuffer<uint8_t> message,
	Message::Attribute attr)
{
	Message::X25519::HeaderPointToPoint header;
	bool parseResult = Message::X25519::ParseHeader(message, header);

	if (!parseResult) {
		return false;
	}

	if (!Message::VerifyFullUserName(header.Source)) {
		return false;
	}

	if (!Message::VerifyFullUserName(header.Destination)) {
		return false;
	}

	if (header.Source == header.Destination) {
		return false;
	}

	String userName;
	String hostName;

	parseResult = Message::SplitFullUserName(
		header.Source,
		userName,
		hostName);

	if (!parseResult) {
		return false;
	}

	if (userName != _name) {
		return false;
	}

	if (hostName != _config->GetHostName()) {
		return false;
	}

	if (crypto_verify32(header.SourceKey.Key, _publicKey.Key)) {
		return false;
	}

	RegisterNewMessage(header, message, attr, false);

	return true;
}

int32_t User::CheckInboundMessage(
	const Message::X25519::HeaderPointToPoint &header,
	const ObjectStorage::ID &messageID)
{
	if (!Message::VerifyFullUserName(header.Source)) {
		return GATE_MESSAGE_HEADER_REJECT_INVALID_HEADER;
	}

	if (!Message::VerifyFullUserName(header.Destination)) {
		return GATE_MESSAGE_HEADER_REJECT_INVALID_HEADER;
	}

	if (header.Source == header.Destination) {
		return GATE_MESSAGE_HEADER_REJECT_INVALID_DESTINATION_USER;
	}

	String userName;
	String hostName;

	bool parseResult = Message::SplitFullUserName(
		header.Destination,
		userName,
		hostName);

	if (!parseResult) {
		return GATE_MESSAGE_HEADER_REJECT_INVALID_DESTINATION_USER;
	}

	if (userName != _name) {
		return GATE_MESSAGE_HEADER_REJECT_INVALID_DESTINATION_USER;
	}

	if (hostName != _config->GetHostName()) {
		return GATE_MESSAGE_HEADER_REJECT_INVALID_DESTINATION_USER;
	}

	if (crypto_verify32(_publicKey.Key, header.DestinationKey.Key)) {
		return GATE_MESSAGE_HEADER_REJECT_INVALID_DESTINATION_KEY;
	}

	uint64_t maxSize = _config->GetMessageSizeLimit();
	uint64_t messageSize =
		header.HeaderSize +
		header.MessageSize +
		sizeof(int32_t) * 2;

	if (messageSize > maxSize) {
		return GATE_MESSAGE_HEADER_REJECT_MESSAGE_TOO_BIG;
	}

	Contact *contact = _contactStorage.GetContact(header.Source);

	if (contact) {
		Contact::BlockStatus blockStat = contact->GetBlockStatus();

		if (blockStat == Contact::BlockStatus::Blocked) {
			return GATE_MESSAGE_HEADER_REJECT_SENDER_BANNED;
		}

		if (blockStat == Contact::BlockStatus::SilentlyBlocked) {
			return GATE_MESSAGE_HEADER_REJECT_SILENTBLOCK;
		}

		if (contact->IsKeyBlocked(header.SourceKey)) {
			return GATE_MESSAGE_HEADER_REJECT_SENDER_KEY_BANNED;
		}
	}

	if (FileExists(_root + "/storage/" + header.Source)) {
		ObjectStorage objectStorage(
			_root + "/storage/" + header.Source,
			_dispatcher);

		if (objectStorage.HasObject(messageID)) {
			return GATE_MESSAGE_HEADER_REJECT_EXISTS;
		}
	}

	return GATE_MESSAGE_HEADER_ACCEPT;
}

void User::DeliverMessage(
	const Message::X25519::HeaderPointToPoint &header,
	const CowBuffer<uint8_t> message)
{
	Message::Attribute attr = Message::Attribute::Unread;
	RegisterNewMessage(header, message, attr, true);
}

CowBuffer<uint8_t> User::GetMessage(
	String peerName,
	const ObjectStorage::ID &messageID,
	Message::Attribute &attr)
{
	ObjectStorage objectStorage(
		_root + "/storage/" + peerName,
		_dispatcher);

	if (!objectStorage.HasObject(messageID)) {
		return CowBuffer<uint8_t>();
	}

	CowBuffer<uint8_t> buffer = objectStorage.ReadObject(messageID);

	if (buffer.Size() <= sizeof(attr)) {
		return CowBuffer<uint8_t>();
	}

	attr = *buffer.SwitchType<Message::Attribute>();
	return buffer.Slice(sizeof(attr), buffer.Size() - sizeof(attr));
}

void User::UpdateMessage(
	String peerName,
	const ObjectStorage::ID &messageID,
	Message::Attribute attr,
	bool value)
{
	// Update message storage.
	ObjectStorage objectStorage(
		_root + "/storage/" + peerName,
		_dispatcher);

	if (!objectStorage.HasObject(messageID)) {
		return;
	}

	CowBuffer<uint8_t> buffer =
		objectStorage.ReadObject(messageID, 0, sizeof(attr));

	Message::Attribute storedAttrs =
		*buffer.SwitchType<Message::Attribute>();

	if (value) {
		storedAttrs = Message::AttributeAction::Set(storedAttrs, attr);
	} else {
		storedAttrs =
			Message::AttributeAction::Clear(storedAttrs, attr);
	}

	*buffer.SwitchType<Message::Attribute>() = storedAttrs;

	objectStorage.UpdateObject(messageID, buffer, 0);

	// Update sequence storage.
	UpdateMessageObject::Data data;

	data.PeerName = peerName;
	data.HeaderHash = messageID;
	data.Attr = attr;
	data.Value = value;

	AddNewObject(UpdateMessageObject::BuildData(data));
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

		CowBuffer<uint8_t> idValueBuffer = itemId.GetValue();

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

void User::RegisterNewMessage(
	const Message::X25519::HeaderPointToPoint &header,
	const CowBuffer<uint8_t> message,
	Message::Attribute attr,
	bool inbound)
{
	ObjectStorage::ID messageID = Crypto::GetHash(
		message.Slice(0, header.HeaderSize),
		(int)ObjectStorage::Constants::IDSize).Pointer();

	String peerName;

	if (inbound) {
		peerName = header.Source;
	} else {
		peerName = header.Destination;
	}

	ObjectStorage objectStorage(
		_root + "/storage/" + peerName,
		_dispatcher);

	if (objectStorage.HasObject(messageID)) {
		return;
	}

	CowBuffer<CowBuffer<uint8_t>> messageAndAttrs(2);
	messageAndAttrs[0] = CowBuffer<uint8_t>(sizeof(attr));
	*messageAndAttrs[0].SwitchType<Message::Attribute>() = attr;

	messageAndAttrs[1] = message;

	objectStorage.WriteObject(messageID, messageAndAttrs);

	MessageObject::Data data;
	data.PeerName = peerName;
	data.HeaderHash = messageID;

	AddNewObject(MessageObject::BuildData(data));

	if (!inbound) {
		UpdateMessage(
			peerName,
			messageID,
			Message::Attribute::Local,
			false);

		UpdateMessage(
			peerName,
			messageID,
			Message::Attribute::InProgress,
			true);

		_userStorage->RegisterMessageForDelivery(header, messageID);
	}
}
