#include "ServerSession.hpp"

#include "ActiveSession.hpp"
#include "Handshake.hpp"
#include "../Common/UnixTime.hpp"
#include "../Message/MessageStorage.hpp"
#include "../Message/Message.hpp"

#include "../Common/Debug.hpp"

ServerSession::~ServerSession()
{
	if (InVoice()) {
		VoicePeer->EndVoice();
		VoicePeer = nullptr;
		VoiceState = VoiceStateInactive;
	}

	if (PeerPublicKey) {
		Pipe->Unregister(PeerPublicKey);
	}

	crypto_wipe(InES.Key, KEY_SIZE);
	crypto_wipe(OutES.Key, KEY_SIZE);
}

bool ServerSession::Process()
{
	if (!CanReceive()) {
		return true;
	}

	switch (State) {
	case ServerStateWaitFirstSyn:
		return ProcessFirstSyn();
	case ServerStateWaitSecondSyn:
		return ProcessSecondSyn();
	case ServerStateActiveSession:
		return ProcessActiveSession();
	}

	return false;
}

bool ServerSession::TimePassed()
{
	int64_t t = GetUnixTime();

	if (t - Time > 10) {
		return false;
	}

	return true;
}

bool ServerSession::ProcessFirstSyn()
{
	CowBuffer<uint8_t> message = Receive();
	message = RemoveScrambler(message);

	Handshake1::Data request;
	bool parseResult = Handshake1::Parse(message, request);

	if (!parseResult) {
		return false;
	}

	if (!Users->HasUser(request.Key)) {
		return false;
	}

	if (Pipe->GetHandler(request.Key)) {
		return false;
	}

	PeerPublicKey = Users->GetUserPublicKey(request.Key);
	SignatureKey = Users->GetUserSignature(PeerPublicKey);

	int64_t prevTime = Users->GetUserAccessTime(PeerPublicKey);
	int64_t currentTime = request.Timestamp;

	if (currentTime <= prevTime) {
		return false;
	}

	bool status = Verify(
		message.Slice(0, KEY_SIZE + sizeof(int64_t)),
		SignatureKey,
		request.Signature.Pointer());

	if (!status) {
		return false;
	}

	Users->UpdateUserAccessTime(PeerPublicKey, currentTime);

	State = ServerStateWaitSecondSyn;

	GenerateSessionKeys(
		PrivateKey,
		PublicKey,
		PeerPublicKey,
		currentTime,
		OutES.Key,
		InES.Key);

	InitNonce(OutES.Nonce);
	memset(InES.Nonce, 0, NONCE_SIZE);

	currentTime += 1;

	Handshake2::Data response;
	response.Key = PublicKey;
	response.Timestamp = currentTime;

	Send(Encrypt(Handshake2::Build(response), OutES));

	return true;
}

bool ServerSession::ProcessSecondSyn()
{
	CowBuffer<uint8_t> encryptedMessage = Receive();
	CowBuffer<uint8_t> plainText = Decrypt(encryptedMessage, InES);

	Handshake3::Data request;
	bool parseResult = Handshake3::Parse(plainText, request);

	if (!parseResult) {
		return false;
	}

	int64_t reference = Users->GetUserAccessTime(PeerPublicKey) + 1;
	int64_t value = request.Timestamp;

	if (value != reference) {
		return false;
	}

	State = ServerStateActiveSession;
	SetInputSizeLimit(1024 * 1024 * 1024);

	Pipe->Register(PeerPublicKey, this);

	return true;
}

bool ServerSession::ProcessActiveSession()
{
	CowBuffer<uint8_t> encryptedMessage = Receive();
	CowBuffer<uint8_t> plainText = Decrypt(encryptedMessage, InES);

	if (plainText.Size() < sizeof(int32_t)) {
		return false;
	}

	int32_t command = *plainText.SwitchType<int32_t>();

	if (command == SESSION_COMMAND_KEEP_ALIVE) {
		return ProcessKeepAlive(plainText);
	} else if (command == SESSION_COMMAND_TEXT_MESSAGE) {
		return ProcessTextMessage(plainText);
	} else if (command == SESSION_COMMAND_LIST_USERS) {
		return ProcessListUsers(plainText);
	} else if (command == SESSION_COMMAND_GET_MESSAGES) {
		return ProcessGetMessages(plainText);
	} else if (command == SESSION_COMMAND_VOICE_INIT) {
		return ProcessVoiceInit(plainText);
	} else if (command == SESSION_COMMAND_VOICE_REQUEST) {
		return ProcessVoiceRequest(plainText);
	} else if (command == SESSION_COMMAND_VOICE_END) {
		return ProcessVoiceEnd(plainText);
	} else if (command == SESSION_COMMAND_VOICE_DATA) {
		return ProcessVoiceData(plainText);
	}

	return false;
}

bool ServerSession::ProcessKeepAlive(const CowBuffer<uint8_t> plainText)
{
	CommandKeepAlive::Command command;
	bool parseResult = CommandKeepAlive::ParseCommand(plainText, command);

	if (!parseResult) {
		return false;
	}

	Send(Encrypt(plainText, OutES));
	return true;
}

bool ServerSession::ProcessTextMessage(const CowBuffer<uint8_t> plainText)
{
	CommandTextMessage::Command command;
	CommandTextMessage::Response response;
	response.Status = CommandTextMessage::ParseCommand(plainText, command);

	if (response.Status == SESSION_RESPONSE_OK)
	{
		Message::Header header;
		bool headerParsed = Message::GetHeader(command.Message, header);

		if (!headerParsed) {
			response.Status =
				SESSION_RESPONSE_ERROR_MESSAGE_TOO_SHORT;
		}

		if (response.Status == SESSION_RESPONSE_OK) {
			if (!Users->HasUser(header.Source)) {
				response.Status =
					SESSION_RESPONSE_ERROR_INVALID_USER;
			} else if (!Users->HasUser(header.Destination)) {
				response.Status =
					SESSION_RESPONSE_ERROR_INVALID_USER;
			} else if (crypto_verify32(
				PeerPublicKey,
				header.Source))
			{
				response.Status =
					SESSION_RESPONSE_ERROR_INVALID_USER;
			}
		}

		if (response.Status == SESSION_RESPONSE_OK) {
			MessageStorage container1(header.Source);
			bool addSuccessful = container1.AddMessage(
				command.Message);

			if (addSuccessful) {
				MessageStorage container2(header.Destination);
				addSuccessful = container2.AddMessage(
					command.Message);
			}

			if (addSuccessful) {
				Pipe->SendMessage(command.Message);
			}
		}
	}

	Send(Encrypt(CommandTextMessage::BuildResponse(response), OutES));
	return true;
}

bool ServerSession::ProcessListUsers(const CowBuffer<uint8_t> plainText)
{
	CommandListUsers::Response response;

	CowBuffer<const uint8_t*> userKeys = Users->ListUsers();
	int32_t userCount = userKeys.Size() - 1;

	if (userCount < 0) {
		return false;
	}

	response.Data.Resize(userCount);

	if (userCount) {
		int index = 0;

		for (int32_t i = 0; i < userCount + 1; i++) {
			const uint8_t *key = userKeys[i];

			if (!crypto_verify32(PeerPublicKey, key)) {
				continue;
			}

			if (index >= userCount) {
				return false;
			}

			response.Data[index].Key = key;
			response.Data[index].Name = Users->GetUserName(key);
			++index;
		}
	}

	Send(Encrypt(CommandListUsers::BuildResponse(response), OutES));
	return true;
}

bool ServerSession::ProcessGetMessages(const CowBuffer<uint8_t> plainText)
{
	CommandGetMessages::Command command;

	bool parseResult = CommandGetMessages::ParseCommand(plainText, command);

	if (!parseResult) {
		return false;
	}

	const int64_t intMax = 0x7fffffffffffffff;

	MessageStorage container(
		PeerPublicKey);

	CowBuffer<CowBuffer<uint8_t>> messages = container.GetMessageRange(
		command.Timestamp,
		intMax);

	for (uint32_t i = 0; i < messages.Size(); i++) {
		SendMessage(messages[i]);
	}

	return true;
}

void ServerSession::SendMessage(const CowBuffer<uint8_t> message)
{
	CommandDeliverMessage::Command command;
	command.Message = message;

	Send(Encrypt(CommandDeliverMessage::BuildCommand(command), OutES));
}

bool ServerSession::InVoice()
{
	return VoicePeer;
}

void ServerSession::SendVoiceFrame(const CowBuffer<uint8_t> frame)
{
	if (!InVoice() || VoiceState != VoiceStateActive) {
		return;
	}

	Send(Encrypt(frame, OutES));
}

void ServerSession::StartVoice(
	const uint8_t *peerKey,
	int64_t timestamp,
	SendMessageHandler *handler)
{
	VoicePeer = handler;
	VoiceState = VoiceStateRinging;

	CommandVoiceRequest::Command command;
	command.Key = peerKey;
	command.Timestamp = timestamp;

	Send(Encrypt(CommandVoiceRequest::BuildCommand(command), OutES));
}

void ServerSession::AcceptVoice()
{
	CommandVoiceInit::Response response;
	response.Status = SESSION_RESPONSE_VOICE_ACCEPT;

	VoiceState = VoiceStateActive;

	Send(Encrypt(CommandVoiceInit::BuildResponse(response), OutES));
}

void ServerSession::DeclineVoice()
{
	CommandVoiceInit::Response response;
	response.Status = SESSION_RESPONSE_VOICE_DECLINE;

	VoiceState = VoiceStateInactive;
	VoicePeer = nullptr;

	Send(Encrypt(CommandVoiceInit::BuildResponse(response), OutES));
}

void ServerSession::EndVoice()
{
	VoiceState = VoiceStateInactive;
	VoicePeer = nullptr;

	Send(Encrypt(CommandVoiceEnd::BuildCommand(), OutES));
}

bool ServerSession::ProcessVoiceInit(const CowBuffer<uint8_t> plainText)
{
	CommandVoiceInit::Command command;
	bool parseResult = CommandVoiceInit::ParseCommand(plainText, command);

	if (!parseResult) {
		return false;
	}

	CommandVoiceInit::Response response;

	if (InVoice()) {
		response.Status = SESSION_RESPONSE_ERROR_YOU_IN_VOICE;
		Send(Encrypt(CommandVoiceInit::BuildResponse(response), OutES));
		return true;
	}

	if (!Users->HasUser(command.Key)) {
		response.Status = SESSION_RESPONSE_ERROR_INVALID_USER;
		Send(Encrypt(CommandVoiceInit::BuildResponse(response), OutES));
		return true;
	}

	SendMessageHandler *handler = Pipe->GetHandler(command.Key);

	if (!handler) {
		response.Status = SESSION_RESPONSE_ERROR_USER_OFFLINE;
		Send(Encrypt(CommandVoiceInit::BuildResponse(response), OutES));
		return true;
	}

	if (handler->InVoice()) {
		response.Status = SESSION_RESPONSE_ERROR_USER_IN_VOICE;
		Send(Encrypt(CommandVoiceInit::BuildResponse(response), OutES));
		return true;
	}

	VoicePeer = handler;
	VoiceState = VoiceStateWaitingForCallee;

	VoicePeer->StartVoice(PeerPublicKey, command.Timestamp, this);

	response.Status = SESSION_RESPONSE_VOICE_RINGING;
	Send(Encrypt(CommandVoiceInit::BuildResponse(response), OutES));
	return true;
}

bool ServerSession::ProcessVoiceRequest(const CowBuffer<uint8_t> plainText)
{
	CommandVoiceRequest::Response response;
	bool parseResult = CommandVoiceRequest::ParseResponse(
		plainText,
		response);

	if (!parseResult) {
		return false;
	}

	if (!VoicePeer || VoiceState != VoiceStateRinging) {
		EndVoice();
		return true;
	}

	if (response.Status == SESSION_RESPONSE_VOICE_ACCEPT) {
		VoicePeer->AcceptVoice();
		VoiceState = VoiceStateActive;
		return true;
	}

	if (response.Status == SESSION_RESPONSE_VOICE_DECLINE) {
		VoicePeer->DeclineVoice();
		VoicePeer = nullptr;
		VoiceState = VoiceStateInactive;
		return true;
	}

	return false;
}

bool ServerSession::ProcessVoiceEnd(const CowBuffer<uint8_t> plainText)
{
	if (!InVoice()) {
		return true;
	}

	VoicePeer->EndVoice();
	VoicePeer = nullptr;
	VoiceState = VoiceStateInactive;
	return true;
}

bool ServerSession::ProcessVoiceData(const CowBuffer<uint8_t> plainText)
{
	if (!InVoice() || VoiceState != VoiceStateActive) {
		return true;
	}

	VoicePeer->SendVoiceFrame(plainText);
	return true;
}
