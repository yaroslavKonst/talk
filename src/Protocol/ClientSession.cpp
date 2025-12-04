#include "ClientSession.hpp"

#include <cstring>

#include "ActiveSession.hpp"
#include "Handshake.hpp"
#include "../Message/Message.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/Exception.hpp"

MessageProcessor::~MessageProcessor()
{ }

ClientSession::ClientSession()
{
	Processor = nullptr;
	State = ClientStateUnconnected;
	TimeState = 0;

	SMUserPointersFirst = nullptr;
	SMUserPointersLast = nullptr;
}

ClientSession::~ClientSession()
{
	crypto_wipe(SignaturePrivateKey, SIGNATURE_PRIVATE_KEY_SIZE);
	crypto_wipe(SignaturePublicKey, SIGNATURE_PUBLIC_KEY_SIZE);
	crypto_wipe(PeerPublicKey, KEY_SIZE);
	crypto_wipe(PublicKey, KEY_SIZE);
	crypto_wipe(PrivateKey, KEY_SIZE);

	for (int i = 0; i < StreamCount; i++) {
		crypto_wipe(Streams[i].InES.Key, KEY_SIZE);
		crypto_wipe(Streams[i].OutES.Key, KEY_SIZE);
	}

	while (SMUserPointersFirst) {
		SMUser *tmp = SMUserPointersFirst;
		SMUserPointersFirst = SMUserPointersFirst->Next;
		delete tmp;
	}
}

void ClientSession::Disconnect()
{
	Close();
	State = ClientSession::ClientStateUnconnected;
	ResetAllSent();
}

bool ClientSession::InitSession()
{
	if (State != ClientStateUnconnected) {
		return false;
	}

	InputSizeLimit = 1024;
	RestrictStreams = true;

	int64_t currentTime = GetUnixTime();

	Handshake1::Data request;
	request.Key = PublicKey;
	request.Timestamp = currentTime;

	Send(ApplyScrambler(Handshake1::Build(request, SignaturePrivateKey)),
		0, false);

	State = ClientStateInitialWaitForServer;

	for (int i = 0; i < StreamCount; i++) {
		GenerateSessionKeys(
			PrivateKey,
			PublicKey,
			PeerPublicKey,
			currentTime + i,
			Streams[i].InES.Key,
			Streams[i].OutES.Key,
			true);

		InitNonce(Streams[i].OutES.Nonce);
		memset(Streams[i].InES.Nonce, 0, NONCE_SIZE);

		OutputStreams[i].SetES(&Streams[i].OutES);
		InputStreams[i].SetES(&Streams[i].InES);
	}

	TimeState = currentTime;

	return true;
}

bool ClientSession::SendMessage(
	const CowBuffer<uint8_t> message,
	void *userPointer)
{
	if (!ConnectedActive()) {
		return false;
	}

	if (message.Size() <= Message::HeaderSize) {
		return false;
	}

	SMUser *userPtr = new SMUser;
	userPtr->Next = nullptr;
	userPtr->Pointer = userPointer;

	if (SMUserPointersFirst) {
		SMUserPointersLast->Next = userPtr;
		SMUserPointersLast = userPtr;
	} else {
		SMUserPointersFirst = userPtr;
		SMUserPointersLast = userPtr;
	}

	CommandTextMessage::Command command;
	command.Message = message;

	Send(CommandTextMessage::BuildCommand(command), 2, true);
	return true;
}

void ClientSession::ResetAllSent()
{
	while (SMUserPointersFirst) {
		SMUser *tmp = SMUserPointersFirst;
		SMUserPointersFirst = SMUserPointersFirst->Next;

		Processor->NotifyDelivery(
			tmp->Pointer,
			SESSION_RESPONSE_ERROR_CONNECTION_LOST);
		delete tmp;
	}

	SMUserPointersLast = nullptr;
}

bool ClientSession::RequestUserList()
{
	if (!ConnectedActive()) {
		return false;
	}

	Send(CommandListUsers::BuildCommand(), 1, true);
	return true;
}

bool ClientSession::RequestNewMessages(int64_t timestamp)
{
	if (!ConnectedActive()) {
		return false;
	}

	CommandGetMessages::Command command;
	command.Timestamp = timestamp;

	Send(CommandGetMessages::BuildCommand(command), 2, true);
	return true;
}

bool ClientSession::InitVoice(const uint8_t *key, int64_t timestamp)
{
	if (!ConnectedActive()) {
		return false;
	}

	CommandVoiceInit::Command command;
	command.Key = key;
	command.Timestamp = timestamp;

	Send(CommandVoiceInit::BuildCommand(command), 1, true);
	return true;
}

bool ClientSession::ResponseVoiceRequest(bool accept)
{
	if (!ConnectedActive()) {
		return false;
	}

	CommandVoiceRequest::Response response;

	if (accept) {
		response.Status = SESSION_RESPONSE_VOICE_ACCEPT;
	} else {
		response.Status = SESSION_RESPONSE_VOICE_DECLINE;
	}

	Send(CommandVoiceRequest::BuildResponse(response), 1, true);
	return true;
}

bool ClientSession::EndVoice()
{
	if (!ConnectedActive()) {
		return false;
	}

	Send(CommandVoiceEnd::BuildCommand(), 1, true);
	return true;
}

bool ClientSession::SendVoiceFrame(const CowBuffer<uint8_t> frame)
{
	if (!ConnectedActive()) {
		return false;
	}

	CommandVoiceData::Command command;
	command.VoiceData = frame;

	Send(CommandVoiceData::BuildCommand(command), 1, true);
	return true;
}

bool ClientSession::Process()
{
	switch (State) {
	case ClientStateUnconnected:
		return false;
	case ClientStateInitialWaitForServer:
		return ProcessInitialWaitForServer();
	case ClientStateActiveSession:
		return ProcessActiveSession();
	}

	return false;
}

bool ClientSession::TimePassed()
{
	if (TimeState) {
		if (GetUnixTime() - TimeState > 10) {
			return false;
		}

		return true;
	}

	if (State != ClientStateActiveSession) {
		return true;
	}

	TimeState = GetUnixTime();
	CommandKeepAlive::Command command;
	command.Timestamp = TimeState;

	Send(CommandKeepAlive::BuildCommand(command), 0, true);
	return true;
}

bool ClientSession::ProcessInitialWaitForServer()
{
	CowBuffer<uint8_t> message = Receive();

	Handshake2::Data request;
	bool parseResult = Handshake2::Parse(message, request);

	if (!parseResult) {
		return false;
	}

	int res = crypto_verify32(request.Key, PeerPublicKey);

	if (res) {
		return false;
	}

	int64_t value = request.Timestamp;

	if (value != TimeState + 1) {
		return false;
	}

	Handshake3::Data response;
	response.Timestamp = value;

	Send(Handshake3::Build(response), 0, true);

	State = ClientStateActiveSession;
	TimeState = 0;

	InputSizeLimit = 1024 * 1024 * 1024;
	RestrictStreams = false;

	int64_t latestTimestamp = Processor->GetLatestReceiveTimestamp();
	RequestNewMessages(latestTimestamp);

	return true;
}

bool ClientSession::ProcessActiveSession()
{
	CowBuffer<uint8_t> plainText = Receive();

	if (plainText.Size() < sizeof(int32_t)) {
		return false;
	}

	int32_t command = *plainText.SwitchType<int32_t>();

	if (command == SESSION_COMMAND_KEEP_ALIVE) {
		return ProcessKeepAlive(plainText);
	} else if (command == SESSION_COMMAND_TEXT_MESSAGE) {
		return ProcessSendMessage(plainText);
	} else if (command == SESSION_COMMAND_DELIVER_MESSAGE) {
		return ProcessDeliverMessage(plainText);
	} else if (command == SESSION_COMMAND_LIST_USERS) {
		return ProcessListUsers(plainText);
	} else if (command == SESSION_COMMAND_VOICE_INIT) {
		return ProcessVoiceInit(plainText);
	} else if (command == SESSION_COMMAND_VOICE_REQUEST) {
		return ProcessVoiceRequest(plainText);
	} else if (command == SESSION_COMMAND_VOICE_END) {
		return ProcessVoiceEnd(plainText);
	} else if (command == SESSION_COMMAND_VOICE_DATA) {
		return ProcessVoiceFrame(plainText);
	}

	return false;
}

bool ClientSession::ProcessKeepAlive(const CowBuffer<uint8_t> plainText)
{
	if (!TimeState) {
		return false;
	}

	CommandKeepAlive::Command command;
	bool parseResult = CommandKeepAlive::ParseCommand(plainText, command);

	if (!parseResult) {
		return false;
	}

	if (TimeState != command.Timestamp) {
		return false;
	}

	TimeState = 0;
	return true;
}

bool ClientSession::ProcessSendMessage(const CowBuffer<uint8_t> plainText)
{
	if (!SMUserPointersFirst) {
		return false;
	}

	CommandTextMessage::Response response;
	bool parseResult = CommandTextMessage::ParseResponse(
		plainText,
		response);

	if (!parseResult) {
		return false;
	}

	void *userPointer = SMUserPointersFirst->Pointer;
	SMUser *tmp = SMUserPointersFirst;
	SMUserPointersFirst = SMUserPointersFirst->Next;
	delete tmp;

	if (!SMUserPointersFirst) {
		SMUserPointersLast = nullptr;
	}

	Processor->NotifyDelivery(userPointer, response.Status);
	return true;
}

bool ClientSession::ProcessDeliverMessage(const CowBuffer<uint8_t> plainText)
{
	CommandDeliverMessage::Command command;
	bool parseResult = CommandDeliverMessage::ParseCommand(
		plainText,
		command);

	if (!parseResult) {
		return false;
	}

	Processor->DeliverMessage(command.Message);
	return true;
}

bool ClientSession::ProcessListUsers(const CowBuffer<uint8_t> plainText)
{
	CommandListUsers::Response response;
	bool parseResult = CommandListUsers::ParseResponse(plainText, response);

	if (!parseResult) {
		return false;
	}

	for (unsigned int i = 0; i < response.Data.Size(); i++) {
		Processor->UpdateUserData(
			response.Data[i].Key,
			response.Data[i].Name);
	}

	return true;
}

bool ClientSession::ProcessVoiceInit(const CowBuffer<uint8_t> plainText)
{
	CommandVoiceInit::Response response;
	bool parseResult = CommandVoiceInit::ParseResponse(plainText, response);

	if (!parseResult) {
		return false;
	}

	Processor->VoiceInitResponse(response.Status);
	return true;
}

bool ClientSession::ProcessVoiceRequest(const CowBuffer<uint8_t> plainText)
{
	CommandVoiceRequest::Command command;
	bool parseResult = CommandVoiceRequest::ParseCommand(
		plainText,
		command);

	if (!parseResult) {
		return false;
	}

	Processor->VoiceRequest(command.Key, command.Timestamp);
	return true;
}

bool ClientSession::ProcessVoiceEnd(const CowBuffer<uint8_t> plainText)
{
	Processor->VoiceEnd();
	return true;
}

bool ClientSession::ProcessVoiceFrame(const CowBuffer<uint8_t> plainText)
{
	CommandVoiceData::Command command;
	bool parseResult = CommandVoiceData::ParseCommand(plainText, command);

	if (!parseResult) {
		return false;
	}

	Processor->ReceiveVoiceFrame(command.VoiceData);
	return true;
}
