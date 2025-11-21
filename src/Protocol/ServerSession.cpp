#include "ServerSession.hpp"

#include "ActiveSession.hpp"
#include "../Common/UnixTime.hpp"
#include "../Message/MessageStorage.hpp"

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

	if (message.Size() != KEY_SIZE + sizeof(int64_t) + SIGNATURE_SIZE) {
		return false;
	}

	if (!Users->HasUser(message.Pointer())) {
		return false;
	}

	if (Pipe->GetHandler(message.Pointer())) {
		return false;
	}

	PeerPublicKey = Users->GetUserPublicKey(message.Pointer());
	SignatureKey = Users->GetUserSignature(PeerPublicKey);

	int64_t prevTime = Users->GetUserAccessTime(PeerPublicKey);
	int64_t currentTime;
	memcpy(&currentTime, message.Pointer() + KEY_SIZE, sizeof(int64_t));

	if (currentTime <= prevTime) {
		return false;
	}

	bool status = Verify(
		message.Slice(0, KEY_SIZE + sizeof(int64_t)),
		SignatureKey,
		message.Pointer() + KEY_SIZE + sizeof(int64_t));

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

	CowBuffer<uint8_t> response(KEY_SIZE + sizeof(currentTime));

	memcpy(response.Pointer(), PublicKey, KEY_SIZE);
	memcpy(response.Pointer() + KEY_SIZE, &currentTime, sizeof(int64_t));

	CowBuffer<uint8_t> cyphertext = Encrypt(response, OutES);

	Send(cyphertext);

	return true;
}

bool ServerSession::ProcessSecondSyn()
{
	CowBuffer<uint8_t> encryptedMessage = Receive();

	CowBuffer<uint8_t> plainText = Decrypt(encryptedMessage, InES);

	if (plainText.Size() != sizeof(int64_t)) {
		return false;
	}

	int64_t reference = Users->GetUserAccessTime(PeerPublicKey) + 1;
	int64_t value;
	memcpy(&value, plainText.Pointer(), sizeof(value));

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

	int32_t command;
	memcpy(&command, plainText.Pointer(), sizeof(command));

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

bool ServerSession::ProcessKeepAlive(CowBuffer<uint8_t> plainText)
{
	if (plainText.Size() != sizeof(int32_t) + sizeof(int64_t)) {
		return false;
	}

	Send(Encrypt(plainText, OutES));
	return true;
}

bool ServerSession::ProcessTextMessage(CowBuffer<uint8_t> plainText)
{
	int32_t status = SESSION_RESPONSE_OK;

	CowBuffer<uint8_t> response(sizeof(int32_t) * 2);
	memcpy(
		response.Pointer(),
		plainText.Pointer(),
		sizeof(int32_t));

	if (plainText.Size() <= sizeof(int32_t) * 2 + KEY_SIZE * 2 +
		sizeof(int64_t))
	{
		status = SESSION_RESPONSE_ERROR_MESSAGE_TOO_SHORT;
	} else {
		CowBuffer<uint8_t> message = plainText.Slice(
			sizeof(int32_t),
			plainText.Size() - sizeof(int32_t));

		if (!Users->HasUser(message.Pointer())) {
			status = SESSION_RESPONSE_ERROR_INVALID_USER;
		}

		if (!Users->HasUser(message.Pointer() + KEY_SIZE)) {
			status = SESSION_RESPONSE_ERROR_INVALID_USER;
		}

		if (status == SESSION_RESPONSE_OK) {
			MessageStorage container1(
				message.Pointer());
			bool addSuccessful = container1.AddMessage(message);

			if (addSuccessful) {
				MessageStorage container2(
					message.Pointer() + KEY_SIZE);
				addSuccessful = container2.AddMessage(message);
			}

			if (addSuccessful) {
				Pipe->SendMessage(message);
			}
		}
	}

	memcpy(
		response.Pointer() + sizeof(int32_t),
		&status,
		sizeof(status));

	Send(Encrypt(response, OutES));
	return true;
}

bool ServerSession::ProcessListUsers(CowBuffer<uint8_t> plainText)
{
	const int32_t entrySize = KEY_SIZE + 55;

	CowBuffer<const uint8_t*> userKeys = Users->ListUsers();
	int32_t userCount = userKeys.Size() - 1;

	if (userCount < 0) {
		return false;
	}

	CowBuffer<uint8_t> message(sizeof(int32_t) * 2 + entrySize * userCount);
	memcpy(message.Pointer(), plainText.Pointer(), sizeof(int32_t));
	memcpy(
		message.Pointer() + sizeof(int32_t),
		&userCount,
		sizeof(userCount));

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

			memcpy(
				message.Pointer() + sizeof(int32_t) * 2 +
				entrySize * index,
				key,
				KEY_SIZE);

			String name = Users->GetUserName(key);
			memcpy(
				message.Pointer() + sizeof(int32_t) * 2 +
				entrySize * index + KEY_SIZE,
				name.CStr(),
				name.Length() + 1);

			++index;
		}
	}

	Send(Encrypt(message, OutES));
	return true;
}

bool ServerSession::ProcessGetMessages(CowBuffer<uint8_t> plainText)
{
	int64_t timestamp;

	if (plainText.Size() != sizeof(int32_t) + sizeof(timestamp)) {
		return false;
	}

	memcpy(
		&timestamp,
		plainText.Pointer() + sizeof(int32_t),
		sizeof(timestamp));

	const int64_t intMax = 0x7fffffffffffffff;

	MessageStorage container(
		PeerPublicKey);

	CowBuffer<CowBuffer<uint8_t>> messages = container.GetMessageRange(
		timestamp,
		intMax);

	for (uint32_t i = 0; i < messages.Size(); i++) {
		SendMessage(messages[i]);
	}

	return true;
}

void ServerSession::SendMessage(CowBuffer<uint8_t> message)
{
	int32_t command = SESSION_COMMAND_DELIVER_MESSAGE;
	CowBuffer<uint8_t> data(message.Size() + sizeof(command));
	memcpy(data.Pointer(), &command, sizeof(command));
	memcpy(
		data.Pointer() + sizeof(command),
		message.Pointer(),
		message.Size());

	Send(Encrypt(data, OutES));
}

bool ServerSession::InVoice()
{
	return VoicePeer;
}

void ServerSession::SendVoiceFrame(CowBuffer<uint8_t> frame)
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

	int32_t command = SESSION_COMMAND_VOICE_REQUEST;

	CowBuffer<uint8_t> request(
		sizeof(command) + KEY_SIZE + sizeof(timestamp));

	memcpy(request.Pointer(), &command, sizeof(command));
	memcpy(request.Pointer() + sizeof(command), peerKey, KEY_SIZE);
	memcpy(
		request.Pointer() + sizeof(command) + KEY_SIZE,
		&timestamp,
		sizeof(timestamp));

	Send(Encrypt(request, OutES));
}

void ServerSession::AcceptVoice()
{
	int32_t command = SESSION_COMMAND_VOICE_INIT;
	int32_t code = SESSION_RESPONSE_VOICE_ACCEPT;
	CowBuffer<uint8_t> response(sizeof(command) + sizeof(code));
	memcpy(response.Pointer(), &command, sizeof(command));
	memcpy(response.Pointer() + sizeof(command), &code, sizeof(code));

	VoiceState = VoiceStateActive;

	Send(Encrypt(response, OutES));
}

void ServerSession::DeclineVoice()
{
	int32_t command = SESSION_COMMAND_VOICE_INIT;
	int32_t code = SESSION_RESPONSE_VOICE_DECLINE;
	CowBuffer<uint8_t> response(sizeof(command) + sizeof(code));
	memcpy(response.Pointer(), &command, sizeof(command));
	memcpy(response.Pointer() + sizeof(command), &code, sizeof(code));

	VoiceState = VoiceStateInactive;
	VoicePeer = nullptr;

	Send(Encrypt(response, OutES));
}

void ServerSession::EndVoice()
{
	int32_t command = SESSION_COMMAND_VOICE_END;
	CowBuffer<uint8_t> response(sizeof(command));
	memcpy(response.Pointer(), &command, sizeof(command));

	VoiceState = VoiceStateInactive;
	VoicePeer = nullptr;

	Send(Encrypt(response, OutES));
}

bool ServerSession::ProcessVoiceInit(CowBuffer<uint8_t> plainText)
{
	if (plainText.Size() != sizeof(int32_t) + KEY_SIZE + sizeof(int64_t)) {
		return false;
	}

	CowBuffer<uint8_t> response(sizeof(int32_t) * 2);
	memcpy(response.Pointer(), plainText.Pointer(), sizeof(int32_t));

	const uint8_t *peerKey = plainText.Pointer() + sizeof(int32_t);

	int64_t timestamp;
	memcpy(
		&timestamp,
		plainText.Pointer() + sizeof(int32_t) + KEY_SIZE,
		sizeof(timestamp));

	if (InVoice()) {
		int32_t code = SESSION_RESPONSE_ERROR_YOU_IN_VOICE;
		memcpy(
			response.Pointer() + sizeof(int32_t),
			&code,
			sizeof(code));
		Send(Encrypt(response, OutES));
		return true;
	}

	if (!Users->HasUser(peerKey)) {
		int32_t code = SESSION_RESPONSE_ERROR_INVALID_USER;
		memcpy(
			response.Pointer() + sizeof(int32_t),
			&code,
			sizeof(code));
		Send(Encrypt(response, OutES));
		return true;
	}

	SendMessageHandler *handler = Pipe->GetHandler(peerKey);

	if (!handler) {
		int32_t code = SESSION_RESPONSE_ERROR_USER_OFFLINE;
		memcpy(
			response.Pointer() + sizeof(int32_t),
			&code,
			sizeof(code));
		Send(Encrypt(response, OutES));
		return true;
	}

	if (handler->InVoice()) {
		int32_t code = SESSION_RESPONSE_ERROR_USER_IN_VOICE;
		memcpy(
			response.Pointer() + sizeof(int32_t),
			&code,
			sizeof(code));
		Send(Encrypt(response, OutES));
		return true;
	}

	VoicePeer = handler;
	VoiceState = VoiceStateWaitingForCallee;

	VoicePeer->StartVoice(PeerPublicKey, timestamp, this);

	int32_t code = SESSION_RESPONSE_VOICE_RINGING;
	memcpy(
		response.Pointer() + sizeof(int32_t),
		&code,
		sizeof(code));
	Send(Encrypt(response, OutES));
	return true;
}

bool ServerSession::ProcessVoiceRequest(CowBuffer<uint8_t> plainText)
{
	if (plainText.Size() != sizeof(int32_t) * 2) {
		return false;
	}

	if (!VoicePeer || VoiceState != VoiceStateRinging) {
		EndVoice();
		return true;
	}

	int32_t code;
	memcpy(&code, plainText.Pointer() + sizeof(int32_t), sizeof(code));

	if (code == SESSION_RESPONSE_VOICE_ACCEPT) {
		VoicePeer->AcceptVoice();
		VoiceState = VoiceStateActive;
		return true;
	}

	if (code == SESSION_RESPONSE_VOICE_DECLINE) {
		VoicePeer->DeclineVoice();
		VoicePeer = nullptr;
		VoiceState = VoiceStateInactive;
		return true;
	}

	return false;
}

bool ServerSession::ProcessVoiceEnd(CowBuffer<uint8_t> plainText)
{
	if (!InVoice()) {
		return true;
	}

	VoicePeer->EndVoice();
	VoicePeer = nullptr;
	VoiceState = VoiceStateInactive;
	return true;
}

bool ServerSession::ProcessVoiceData(CowBuffer<uint8_t> plainText)
{
	if (!InVoice() || VoiceState != VoiceStateActive) {
		return true;
	}

	VoicePeer->SendVoiceFrame(plainText);
	return true;
}
