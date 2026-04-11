#include "ServerSession.hpp"

#include <unistd.h>
#include <cstdlib>
#include <sys/socket.h>

#include "User.hpp"
#include "ObjectType.hpp"
#include "../Protocol/SessionParser.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/Log.hpp"
#include "../Common/Exception.hpp"

ServerSession::ServerSession(
	int fd,
	ServerSessionStorage *storage,
	Config *config,
	EventDispatcher *dispatcher,
	EncryptedStream *outES,
	EncryptedStream *inES,
	uint8_t outScramblerInit,
	uint8_t inScramblerInit)
{
	SetInterval(10);
	SetTimestamp(GetUnixTime());

	_dispatcher = dispatcher;
	_fd = fd;
	_storage = storage;
	_config = config;

	_inES = *inES;
	_outES = *outES;

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

	int32_t command = *buffer.SwitchType<int32_t>();

	switch (command) {
	case SESSION_COMMAND_KEEP_ALIVE:
		return ProcessKeepAlive(buffer);
	case SESSION_COMMAND_GET_HOST_NAME:
		return ProcessGetHostName();
	case SESSION_COMMAND_ADD_CONTACT:
		return ProcessAddContact(buffer);
	case SESSION_COMMAND_REQUEST_ID:
		return ProcessRequestID(buffer);
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
	SessionLog("Requested host name.");

	CommandGetHostName::Response response;
	response.Name = _config->GetHostName();
	CowBuffer<uint8_t> buffer = CommandGetHostName::BuildResponse(response);

	_protocol->Send(buffer, 0);
	return true;
}

bool ServerSession::ProcessAddContact(const CowBuffer<uint8_t> buffer)
{
	CommandAddContact::Command command;
	bool parseResult = CommandAddContact::ParseCommand(buffer, command);

	if (!parseResult) {
		return false;
	}

	SessionLog("Requested add contact " + command.ContactName + ".");

	_storage->AddContact(command.ContactName);
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

	CowBuffer<uint8_t> object = objectStorage->ReadObject(requiredObjectId);
	SendObject(object);
	SendID(requiredObjectId);
	SendIDRequest();
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
	}
}

void ServerSession::SendAddContact(const CowBuffer<uint8_t> object)
{
	String contactName(
		object.SwitchType<char>(
			sizeof(int32_t) +
			(int)ObjectStorage::Constants::IDSize),
		object.Size() - sizeof(int32_t) -
			(int)ObjectStorage::Constants::IDSize);

	CommandAddContact::Command command;
	command.ContactName = contactName;

	SessionLog("Sent add contact " + contactName + ".");

	_protocol->Send(CommandAddContact::BuildCommand(command), 1);
}

void ServerSession::SessionLog(String message)
{
	uint64_t sessionIndex = (uint64_t)this;

	Log("Session " + ToString(sessionIndex) + " of " +
		_storage->GetName() + ": " + message);
}
