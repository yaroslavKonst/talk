#include "ClientSession.hpp"

#include "../Protocol/SessionParser.hpp"
#include "../Common/Exception.hpp"
#include "../Common/Endianness.hpp"
#include "../Common/UnixTime.hpp"

ClientSession::ClientSession(
	Root *root,
	int fd,
	Crypto::X25519::EncryptedStream &outES,
	Crypto::X25519::EncryptedStream &inES,
	uint8_t outScramblerInit,
	uint8_t inScramblerInit)
{
	_root = root;

	_fd = fd;
	_inES = inES;
	_outES = outES;

	_keepAliveTimestamp = 0;
	_contactListProcessor = nullptr;
	_accountSettingsProcessor = nullptr;

	_messageSizeLimit = 2048;

	_protocol = new SessionProtocol(
		fd,
		&_outES,
		&_inES,
		outScramblerInit,
		inScramblerInit);

	RequestHostName();
	RequestMessageSizeLimit();
}

ClientSession::~ClientSession()
{
	delete _protocol;

	if (_contactListProcessor) {
		_contactListProcessor->ProcessContactList(
			false,
			CommandListContacts::Response());
		_contactListProcessor = nullptr;
	}
}

bool ClientSession::RequestRead()
{
	return true;
}

bool ClientSession::RequestWrite()
{
	return _protocol->RequestWrite();
}

bool ClientSession::ProcessRead()
{
	bool success = _protocol->Read();

	if (!success) {
		return false;
	}

	if (!_protocol->CanReceive()) {
		return true;
	}

	return ProcessInput(_protocol->Receive());
}

bool ClientSession::ProcessWrite()
{
	return _protocol->Write();
}

uint64_t ClientSession::GetMaxMessageSize()
{
	return _messageSizeLimit;
}

bool ClientSession::InitKeepAlive()
{
	if (_keepAliveTimestamp) {
		return false;
	}

	SendKeepAlive();
	return true;
}

void ClientSession::AddContact(String name)
{
	CommandAddContact::Command command;
	command.ContactName = name;

	_protocol->Send(CommandAddContact::BuildCommand(command), 0);
}

void ClientSession::UpdateContactKey(
	String contactName,
	const Crypto::X25519::PublicKeyContainer &key,
	bool validated,
	bool blocked,
	bool setAsDefault)
{
	CommandUpdateContactKey::Command command;
	command.ContactName = contactName;
	command.Key = key;
	command.Validated = validated;
	command.Blocked = blocked;
	command.SetAsDefault = setAsDefault;

	_protocol->Send(CommandUpdateContactKey::BuildCommand(command), 0);
}

void ClientSession::BlockContact(String contactName, Contact::BlockStatus block)
{
	CommandBlockContact::Command command;
	command.ContactName = contactName;
	command.BlockStatus = (uint8_t)block;

	_protocol->Send(CommandBlockContact::BuildCommand(command), 0);
}

void ClientSession::RemoveContact(String name)
{
	CommandRemoveContact::Command command;
	command.ContactName = name;

	_protocol->Send(CommandRemoveContact::BuildCommand(command), 0);
}

void ClientSession::ListContacts()
{
	_protocol->Send(CommandListContacts::BuildCommand(), 0);
}

void ClientSession::SetContactListProcessor(
	NetworkEventProcessor::ContactListProcessor *processor)
{
	_contactListProcessor = processor;
}

void ClientSession::SendMessage(const CowBuffer<uint8_t> message)
{
	CommandSendMessage::Command command;
	command.Message = message;
	command.Attr = Message::Attribute::Unread;

	_protocol->Send(CommandSendMessage::BuildCommand(command), 1);
}

void ClientSession::UpdateMessage(
	String peerName,
	const ObjectStorage::ID &messageID,
	Message::Attribute attr,
	bool value)
{
	CommandUpdateMessage::Command command;
	command.PeerName = peerName;
	command.HeaderHash = messageID.GetValue();
	command.Attr = attr;
	command.AttrValue = value;

	_protocol->Send(CommandUpdateMessage::BuildCommand(command), 1);
}

void ClientSession::GetAccountSettings()
{
	_protocol->Send(CommandGetAccountSettings::BuildCommand(), 0);
}

void ClientSession::SetAccountSettings(bool allowMessages, bool allowCalls)
{
	CommandSetAccountSettings::Command command;
	command.AllowMessagesOnlyFromContactList = allowMessages;
	command.AllowCallsOnlyFromContactList = allowCalls;

	_protocol->Send(CommandSetAccountSettings::BuildCommand(command), 0);
}

void ClientSession::SetAccountSettingsProcessor(
	NetworkEventProcessor::AccountSettingsProcessor *processor)
{
	_accountSettingsProcessor = processor;
}

void ClientSession::SendStreamInit(const CowBuffer<uint8_t> buffer)
{
	CommandStreamInit::Command command;
	command.InitRequest = buffer;

	_protocol->Send(CommandStreamInit::BuildCommand(command), 0);
}

void ClientSession::SendStreamEnd()
{
	_protocol->Send(CommandStreamEnd::BuildCommand(), 0);
}

void ClientSession::SendStreamRequest(const CowBuffer<uint8_t> buffer)
{
	CommandStreamRequest::Response response;
	response.InitResponse = buffer;

	_protocol->Send(CommandStreamRequest::BuildResponse(response), 0);
}

bool ClientSession::ProcessInput(const CowBuffer<uint8_t> buffer)
{
	if (buffer.Size() < sizeof(int32_t)) {
		return false;
	}

	int32_t command = SetProtoEndian(*buffer.SwitchType<int32_t>());

	switch (command) {
	case SESSION_COMMAND_KEEP_ALIVE:
		return ProcessKeepAlive(buffer);
	case SESSION_COMMAND_GET_HOST_NAME:
		return ProcessGetHostName(buffer);
	case SESSION_COMMAND_GET_MAX_MESSAGE_SIZE:
		return ProcessGetMaxMessageSize(buffer);
	case SESSION_COMMAND_ADD_CONTACT:
		return ProcessAddContact(buffer);
	case SESSION_COMMAND_REQUEST_ID:
		return ProcessRequestID();
	case SESSION_COMMAND_UPDATE_ID:
		return ProcessUpdateID(buffer);
	case SESSION_COMMAND_GET_ACCOUNT_SETTINGS:
		return ProcessGetAccountSettings(buffer);
	case SESSION_COMMAND_UPDATE_CONTACT_KEY:
		return ProcessUpdateContactKey(buffer);
	case SESSION_COMMAND_BLOCK_CONTACT:
		return ProcessBlockContact(buffer);
	case SESSION_COMMAND_REMOVE_CONTACT:
		return ProcessRemoveContact(buffer);
	case SESSION_COMMAND_LIST_CONTACTS:
		return ProcessListContacts(buffer);
	case SESSION_COMMAND_OFFER_MESSAGE:
		return ProcessOfferMessage(buffer);
	case SESSION_COMMAND_SEND_MESSAGE:
		return ProcessSendMessage(buffer);
	case SESSION_COMMAND_UPDATE_MESSAGE:
		return ProcessUpdateMessage(buffer);
	case SESSION_COMMAND_STREAM_INIT:
		return ProcessStreamInit(buffer);
	case SESSION_COMMAND_STREAM_END:
		return ProcessStreamEnd();
	case SESSION_COMMAND_STREAM_REQUEST:
		return ProcessStreamRequest(buffer);
	case SESSION_COMMAND_STREAM_RESPONSE:
		return ProcessStreamResponse(buffer);
	default:
		return false;
	}
}

bool ClientSession::ProcessKeepAlive(const CowBuffer<uint8_t> buffer)
{
	CommandKeepAlive::Command request;
	bool parseResult = CommandKeepAlive::ParseCommand(buffer, request);

	if (!parseResult) {
		return false;
	}

	if (request.Timestamp != _keepAliveTimestamp) {
		return false;
	}

	_keepAliveTimestamp = 0;
	return true;
}

void ClientSession::SendKeepAlive()
{
	_keepAliveTimestamp = GetUnixTime();

	CommandKeepAlive::Command request;
	request.Timestamp = _keepAliveTimestamp;

	_protocol->Send(CommandKeepAlive::BuildCommand(request), 0);
}

void ClientSession::RequestHostName()
{
	_protocol->Send(CommandGetHostName::BuildCommand(), 0);
}

bool ClientSession::ProcessGetHostName(const CowBuffer<uint8_t> buffer)
{
	CommandGetHostName::Response response;
	bool parseResult = CommandGetHostName::ParseResponse(buffer, response);

	if (!parseResult) {
		return false;
	}

	_root->Conf->SetHostName(response.Name);
	_root->Conf->Save();
	_root->Ui->Redraw();
	return true;
}

void ClientSession::RequestMessageSizeLimit()
{
	_protocol->Send(CommandGetMaxMessageSize::BuildCommand(), 0);
}

bool ClientSession::ProcessGetMaxMessageSize(const CowBuffer<uint8_t> buffer)
{
	CommandGetMaxMessageSize::Response response;
	bool parseResult = CommandGetMaxMessageSize::ParseResponse(
		buffer,
		response);

	if (!parseResult) {
		return false;
	}

	_messageSizeLimit = response.Value;
	_protocol->SetInputSizeLimit(_messageSizeLimit);
	return true;
}

bool ClientSession::ProcessRequestID()
{
	CommandRequestID::Response response;
	response.Id = _root->Messages->GetKnownID();

	_protocol->Send(CommandRequestID::BuildResponse(response), 1);
	return true;
}

bool ClientSession::ProcessUpdateID(const CowBuffer<uint8_t> buffer)
{
	CommandUpdateID::Command command;
	bool parseResult = CommandUpdateID::ParseCommand(buffer, command);

	if (!parseResult) {
		return false;
	}

	_root->Messages->SetKnownID(command.Id);
	return true;
}

bool ClientSession::ProcessGetAccountSettings(const CowBuffer<uint8_t> buffer)
{
	CommandGetAccountSettings::Response response;
	bool parseResult = CommandGetAccountSettings::ParseResponse(
		buffer,
		response);

	if (!parseResult) {
		return false;
	}

	if (_accountSettingsProcessor) {
		_accountSettingsProcessor->ReceiveAccountSettings(
			response.AllowMessagesOnlyFromContactList,
			response.AllowCallsOnlyFromContactList);
	}

	return true;
}

bool ClientSession::ProcessAddContact(const CowBuffer<uint8_t> buffer)
{
	CommandAddContact::Command command;
	bool parseResult = CommandAddContact::ParseCommand(buffer, command);

	if (!parseResult) {
		return false;
	}

	if (!Message::VerifyFullUserName(command.ContactName)) {
		return false;
	}

	_root->Messages->GetContactStorage()->AddNewContact(
		command.ContactName);
	_root->Ui->Redraw();
	return true;
}

bool ClientSession::ProcessUpdateContactKey(const CowBuffer<uint8_t> buffer)
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

	ContactStorage *storage = _root->Messages->GetContactStorage();

	if (!storage->HasContact(command.ContactName)) {
		storage->AddNewContact(command.ContactName);
	}

	Contact *contact = storage->GetContact(command.ContactName);

	contact->UpdateKey(
		command.Key,
		command.Validated,
		command.Blocked);

	if (command.SetAsDefault) {
		contact->SetDefaultKey(command.Key);
	}

	_root->Ui->Redraw();
	return true;
}

bool ClientSession::ProcessBlockContact(const CowBuffer<uint8_t> buffer)
{
	CommandBlockContact::Command command;
	bool parseResult = CommandBlockContact::ParseCommand(
		buffer,
		command);

	if (!parseResult) {
		return false;
	}

	if (!Message::VerifyFullUserName(command.ContactName)) {
		return false;
	}

	ContactStorage *storage = _root->Messages->GetContactStorage();

	if (!storage->HasContact(command.ContactName)) {
		storage->AddNewContact(command.ContactName);
	}

	Contact *contact = storage->GetContact(command.ContactName);

	contact->SetBlockStatus((Contact::BlockStatus)command.BlockStatus);
	_root->Ui->Redraw();
	return true;
}

bool ClientSession::ProcessRemoveContact(const CowBuffer<uint8_t> buffer)
{
	CommandRemoveContact::Command command;
	bool parseResult = CommandRemoveContact::ParseCommand(buffer, command);

	if (!parseResult) {
		return false;
	}

	if (!Message::VerifyFullUserName(command.ContactName)) {
		return false;
	}

	_root->Messages->GetContactStorage()->RemoveContact(
		command.ContactName);
	_root->Ui->Redraw();
	return true;
}

bool ClientSession::ProcessListContacts(const CowBuffer<uint8_t> buffer)
{
	CommandListContacts::Response response;
	bool parseResult = CommandListContacts::ParseResponse(
		buffer,
		response);

	if (!parseResult) {
		return false;
	}

	if (_contactListProcessor) {
		_contactListProcessor->ProcessContactList(true, response);
	}

	return true;
}

bool ClientSession::ProcessOfferMessage(const CowBuffer<uint8_t> buffer)
{
	CommandOfferMessage::Command command;
	bool parseResult = CommandOfferMessage::ParseCommand(buffer, command);

	if (!parseResult) {
		return false;
	}

	if (!Message::VerifyFullUserName(command.PeerName)) {
		return false;
	}

	bool messageExists = _root->Messages->HasMessage(
		command.PeerName,
		command.HeaderHash.Pointer());

	CommandOfferMessage::Response response;
	response.Answer = !messageExists;

	_protocol->Send(CommandOfferMessage::BuildResponse(response), 1);
	return true;
}

bool ClientSession::ProcessSendMessage(const CowBuffer<uint8_t> buffer)
{
	CommandSendMessage::Command command;
	bool parseResult = CommandSendMessage::ParseCommand(buffer, command);

	if (!parseResult) {
		return false;
	}

	_root->Messages->DeliverMessage(command.Message, command.Attr);
	return true;
}

bool ClientSession::ProcessUpdateMessage(const CowBuffer<uint8_t> buffer)
{
	CommandUpdateMessage::Command command;
	bool parseResult = CommandUpdateMessage::ParseCommand(buffer, command);

	if (!parseResult) {
		return false;
	}

	if (!Message::VerifyFullUserName(command.PeerName)) {
		return false;
	}

	_root->Messages->UpdateMessage(
		command.PeerName,
		command.HeaderHash.Pointer(),
		command.Attr,
		command.AttrValue);

	return true;
}

bool ClientSession::ProcessStreamInit(const CowBuffer<uint8_t> buffer)
{
	CommandStreamInit::Response response;
	bool parseResult = CommandStreamInit::ParseResponse(buffer, response);

	if (!parseResult) {
		return false;
	}

	_root->Voice->ProcessInitResponse(response.Status);
	return true;
}

bool ClientSession::ProcessStreamEnd()
{
	_root->Voice->StreamEnd();
	return true;
}

bool ClientSession::ProcessStreamRequest(const CowBuffer<uint8_t> buffer)
{
	CommandStreamRequest::Command command;
	bool parseResult = CommandStreamRequest::ParseCommand(buffer, command);

	if (!parseResult) {
		return false;
	}

	_root->Voice->ProcessInit(command.InitRequest);
	return true;
}

bool ClientSession::ProcessStreamResponse(const CowBuffer<uint8_t> buffer)
{
	CommandStreamResponse::Command command;
	bool parseResult = CommandStreamResponse::ParseCommand(buffer, command);

	if (!parseResult) {
		return false;
	}

	_root->Voice->ProcessPeerResponse(command.InitResponse);
	return true;
}
