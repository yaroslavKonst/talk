#include "ServerSession.hpp"

#include <unistd.h>
#include <cstdlib>
#include <sys/socket.h>

#include "User.hpp"
#include "ObjectType.hpp"
#include "../Protocol/SessionParser.hpp"
#include "../Protocol/ParserHelpers.hpp"
#include "../Common/Endianness.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/Log.hpp"
#include "../Common/Exception.hpp"

ServerSession::ServerSession(
	int fd,
	ServerSessionStorage *storage,
	Config *config,
	EventDispatcher *dispatcher,
	const Crypto::X25519::EncryptedStream &outES,
	const Crypto::X25519::EncryptedStream &inES,
	uint8_t outScramblerInit,
	uint8_t inScramblerInit)
{
	SetInterval(10);
	SetTimestamp(GetUnixTime());

	_dispatcher = dispatcher;
	_fd = fd;
	_storage = storage;
	_config = config;

	_inES = inES;
	_outES = outES;

	_protocol = new SessionProtocol(
		_fd,
		&_outES,
		&_inES,
		outScramblerInit,
		inScramblerInit);

	_objectTransmissionActive = false;

	_protocol->SetInputSizeLimit(_config->GetMessageSizeLimit());

	_dispatcher->RegisterDescriptorProcessor(this);
	_dispatcher->RegisterTimeProcessor(this);
	_config->RegisterConfigUser(this);

	SessionLog("Start session.");

	SendObjects();
}

ServerSession::~ServerSession()
{
	SessionLog("End session.");

	_config->UnregisterConfigUser(this);
	_dispatcher->UnregisterDescriptorProcessor(this);
	_dispatcher->UnregisterTimeProcessor(this);

	delete _protocol;

	if (_fd != -1) {
		shutdown(_fd, SHUT_RDWR);
		close(_fd);
		_fd = -1;
	}
}

void ServerSession::ReloadConfig()
{
	_protocol->SetInputSizeLimit(_config->GetMessageSizeLimit());
	ProcessGetHostName();
	ProcessGetMaxMessageSize();
}

bool ServerSession::RequestRead()
{
	return true;
}

bool ServerSession::RequestWrite()
{
	return _protocol->RequestWrite();
}

void ServerSession::ProcessRead()
{
	SetTimestamp(GetUnixTime());

	bool success = _protocol->Read();

	if (!success) {
		_storage->MarkSessionForRemoval(this);
		return;
	}

	if (!_protocol->CanReceive()) {
		return;
	}

	success = ProcessInput(_protocol->Receive());

	if (!success) {
		_storage->MarkSessionForRemoval(this);
	}
}

void ServerSession::ProcessWrite()
{
	SetTimestamp(GetUnixTime());

	bool success = _protocol->Write();

	if (!success) {
		_storage->MarkSessionForRemoval(this);
	}
}

void ServerSession::ProcessTimeEvent()
{
	_storage->MarkSessionForRemoval(this);
}

void ServerSession::SendObjects()
{
	if (!_objectTransmissionActive) {
		InitObjectTransmission();
	}
}

bool ServerSession::ProcessInput(const CowBuffer<uint8_t> buffer)
{
	if (buffer.Size() < sizeof(int32_t)) {
		SessionLog("Protocol violation.");
		return false;
	}

	int32_t command = SetProtoEndian(*buffer.SwitchType<int32_t>());

	switch (command) {
	case SESSION_COMMAND_KEEP_ALIVE:
		return ProcessKeepAlive(buffer);
	case SESSION_COMMAND_GET_HOST_NAME:
		return ProcessGetHostName();
	case SESSION_COMMAND_GET_MAX_MESSAGE_SIZE:
		return ProcessGetMaxMessageSize();
	case SESSION_COMMAND_REQUEST_ID:
		return ProcessRequestID(buffer);
	case SESSION_COMMAND_ADD_CONTACT:
		return ProcessAddContact(buffer);
	case SESSION_COMMAND_UPDATE_CONTACT_KEY:
		return ProcessUpdateContactKey(buffer);
	case SESSION_COMMAND_BLOCK_CONTACT:
		return ProcessBlockContact(buffer);
	case SESSION_COMMAND_LIST_CONTACTS:
		return ProcessListContacts();
	case SESSION_COMMAND_SEND_MESSAGE:
		return ProcessSendMessage(buffer);
	case SESSION_COMMAND_OFFER_MESSAGE:
		return ProcessOfferMessage(buffer);
	default:
		SessionLog("Unknown command.");
		return false;
	}
}

bool ServerSession::ProcessKeepAlive(const CowBuffer<uint8_t> buffer)
{
	CommandKeepAlive::Command request;
	bool parseResult = CommandKeepAlive::ParseCommand(buffer, request);

	if (!parseResult) {
		return false;
	}

	_protocol->Send(buffer, 0);
	return true;
}

bool ServerSession::ProcessGetHostName()
{
	SessionLog("Host name is requested.");

	CommandGetHostName::Response response;
	response.Name = _config->GetHostName();
	CowBuffer<uint8_t> buffer = CommandGetHostName::BuildResponse(response);

	_protocol->Send(buffer, 0);
	return true;
}

bool ServerSession::ProcessGetMaxMessageSize()
{
	SessionLog("Max message size is requested.");

	CommandGetMaxMessageSize::Response response;
	response.Value = _config->GetMessageSizeLimit();

	_protocol->Send(CommandGetMaxMessageSize::BuildResponse(response), 0);
	return true;
}

bool ServerSession::ProcessAddContact(const CowBuffer<uint8_t> buffer)
{
	CommandAddContact::Command command;
	bool parseResult = CommandAddContact::ParseCommand(buffer, command);

	if (!parseResult) {
		return false;
	}

	if (!Message::VerifyFullUserName(command.ContactName)) {
		return false;
	}

	SessionLog("Requested add contact " + command.ContactName + ".");

	_storage->AddContact(command.ContactName);
	return true;
}

bool ServerSession::ProcessUpdateContactKey(const CowBuffer<uint8_t> buffer)
{
	CommandUpdateContactKey::Command command;
	bool parseResult = CommandUpdateContactKey::ParseCommand(
		buffer,
		command);

	if (!parseResult) {
		return false;
	}

	if (!Message::VerifyFullUserName(command.ContactName)) {
		return false;
	}

	SessionLog("Requested key update for " + command.ContactName + ".");

	_storage->UpdateContactKey(
		command.ContactName,
		command.Key,
		command.Validated,
		command.Blocked,
		command.SetAsDefault);
	return true;
}

bool ServerSession::ProcessBlockContact(const CowBuffer<uint8_t> buffer)
{
	CommandBlockContact::Command command;
	bool parseResult = CommandBlockContact::ParseCommand(buffer, command);

	if (!parseResult) {
		return false;
	}

	if (!Message::VerifyFullUserName(command.ContactName)) {
		return false;
	}

	SessionLog("Requested block for " + command.ContactName + ".");

	_storage->BlockContact(
		command.ContactName,
		(Contact::BlockStatus)command.BlockStatus);
	return true;
}

bool ServerSession::ProcessListContacts()
{
	CommandListContacts::Response response;
	response.Data = _storage->GetContactList();

	_protocol->Send(CommandListContacts::BuildResponse(response), 0);
	return true;
}

bool ServerSession::ProcessSendMessage(const CowBuffer<uint8_t> buffer)
{
	CommandSendMessage::Command command;
	bool parseResult = CommandSendMessage::ParseCommand(buffer, command);

	if (!parseResult) {
		return false;
	}

	bool sendStatus = _storage->SendMessage(command.Message);

	return sendStatus;
}

bool ServerSession::ProcessOfferMessage(const CowBuffer<uint8_t> buffer)
{
	CommandOfferMessage::Response response;
	bool parseResult = CommandOfferMessage::ParseResponse(buffer, response);

	if (!parseResult) {
		return false;
	}

	if (!_offeredMessagePeerName.Length() || _offeredMessageID.IsZero()) {
		return false;
	}

	if (response.Answer) {
		SendMessage();
	} else {
		_offeredMessagePeerName = String();
		_offeredMessageID = ObjectStorage::ID();

		SendID(_currentObjectID);
		SendIDRequest();
	}

	return true;
}

void ServerSession::InitObjectTransmission()
{
	if (_objectTransmissionActive) {
		return;
	}

	_objectTransmissionActive = true;
	SendIDRequest();
}

void ServerSession::ObjectTransmissionStep(const ObjectStorage::ID &id)
{
	ObjectStorage *objectStorage = _storage->GetObjectStorage();

	if (!objectStorage->HasRef(ROOT_REF)) {
		_objectTransmissionActive = false;
		return;
	}

	ObjectStorage::ID requiredObjectId;

	if (!objectStorage->HasObject(id)) {
		requiredObjectId = objectStorage->GetRef(ROOT_REF);
	} else {
		CowBuffer<uint8_t> object = objectStorage->ReadObject(id);
		requiredObjectId.SetValue(object.Pointer(sizeof(int32_t)));
	}

	if (requiredObjectId.IsZero()) {
		_objectTransmissionActive = false;
		return;
	}

	_currentObjectID = requiredObjectId;

	CowBuffer<uint8_t> object = objectStorage->ReadObject(requiredObjectId);
	SendObject(object);
}

void ServerSession::SendIDRequest()
{
	SessionLog("Sent ID request.");
	_protocol->Send(CommandRequestID::BuildCommand(), 1);
}

void ServerSession::SendID(const ObjectStorage::ID &id)
{
	SessionLog("Sent ID update.");
	CommandUpdateID::Command command;
	command.Id = id;

	_protocol->Send(CommandUpdateID::BuildCommand(command), 1);
}

bool ServerSession::ProcessRequestID(const CowBuffer<uint8_t> buffer)
{
	SessionLog("Received ID.");
	
	if (!_objectTransmissionActive) {
		return false;
	}

	CommandRequestID::Response response;
	bool parseResult = CommandRequestID::ParseResponse(buffer, response);

	if (!parseResult) {
		return false;
	}

	ObjectTransmissionStep(response.Id);
	return true;
}

void ServerSession::SendObject(const CowBuffer<uint8_t> object)
{
	int32_t objectType = *object.SwitchType<int32_t>();

	switch (objectType) {
	case (int)ObjectType::NewContact:
		SendAddContact(object);
		break;
	case (int)ObjectType::UpdateContactKey:
		SendUpdateContactKey(object);
		break;
	case (int)ObjectType::BlockContact:
		SendBlockContact(object);
		break;
	case (int)ObjectType::Message:
		SendOfferMessage(object);
		break;
	case (int)ObjectType::UpdateMessage:
		SendMessageUpdate(object);
		break;
	default:
		THROW("Unsupported database entry type.");
	}
}

void ServerSession::SendAddContact(const CowBuffer<uint8_t> object)
{
	NewContactObject::Data data;
	bool parseResult = NewContactObject::ParseData(object, data);

	if (!parseResult) {
		THROW("Corrupt database entry.");
	}

	CommandAddContact::Command command;
	command.ContactName = data.ContactName;

	SessionLog("Sent add contact " + command.ContactName + ".");

	_protocol->Send(CommandAddContact::BuildCommand(command), 1);

	SendID(_currentObjectID);
	SendIDRequest();
}

void ServerSession::SendUpdateContactKey(const CowBuffer<uint8_t> object)
{
	UpdateContactKeyObject::Data data;
	bool parseResult = UpdateContactKeyObject::ParseData(object, data);

	if (!parseResult) {
		THROW("Corrupt database entry.");
	}

	CommandUpdateContactKey::Command command;
	command.ContactName = data.ContactName;
	command.Key = data.Key;
	command.Validated = data.Validated;
	command.Blocked = data.Blocked;
	command.SetAsDefault = data.SetAsDefault;

	SessionLog("Sent update contact key for " + command.ContactName + ".");

	_protocol->Send(CommandUpdateContactKey::BuildCommand(command), 1);

	SendID(_currentObjectID);
	SendIDRequest();
}

void ServerSession::SendBlockContact(const CowBuffer<uint8_t> object)
{
	BlockContactObject::Data data;
	bool parseResult = BlockContactObject::ParseData(object, data);

	if (!parseResult) {
		THROW("Corrupt database entry.");
	}

	CommandBlockContact::Command command;
	command.ContactName = data.ContactName;
	command.BlockStatus = data.BlockStatus;

	SessionLog("Sent block contact for " + command.ContactName + ".");

	_protocol->Send(CommandBlockContact::BuildCommand(command), 1);

	SendID(_currentObjectID);
	SendIDRequest();
}

void ServerSession::SendOfferMessage(const CowBuffer<uint8_t> object)
{
	MessageObject::Data data;
	bool parseResult = MessageObject::ParseData(object, data);

	if (!parseResult) {
		THROW("Corrupt database entry.");
	}

	CommandOfferMessage::Command command;
	command.PeerName = data.PeerName;
	command.HeaderHash = data.HeaderHash.GetValue();

	_protocol->Send(CommandOfferMessage::BuildCommand(command), 1);
}

void ServerSession::SendMessage()
{
	CommandSendMessage::Command command;
	command.Message = _storage->GetMessage(
		_offeredMessagePeerName,
		_offeredMessageID);

	if (command.Message.Size()) {
		_protocol->Send(CommandSendMessage::BuildCommand(command), 1);
	}

	_offeredMessagePeerName = String();
	_offeredMessageID = ObjectStorage::ID();

	SendID(_currentObjectID);
	SendIDRequest();
}

void ServerSession::SendMessageUpdate(const CowBuffer<uint8_t> object)
{
	UpdateMessageObject::Data data;
	bool parseResult = UpdateMessageObject::ParseData(object, data);

	if (!parseResult) {
		THROW("Corrupt database entry.");
	}

	CommandUpdateMessage::Command command;
	command.PeerName = data.PeerName;
	command.HeaderHash = data.HeaderHash.GetValue();

	command.Attr = data.Attr;
	command.AttrValue = data.Value;

	_protocol->Send(CommandUpdateMessage::BuildCommand(command), 1);

	SendID(_currentObjectID);
	SendIDRequest();
}

void ServerSession::SessionLog(String message)
{
	uint64_t sessionIndex = (uint64_t)this;

	Log(
		"Session " + ToString(sessionIndex) + " of " +
			_storage->GetName(),
		message);
}
