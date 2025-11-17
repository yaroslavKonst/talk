#include "ClientSession.hpp"

#include "ActiveSession.hpp"
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
	crypto_wipe(InES.Key, KEY_SIZE);
	crypto_wipe(OutES.Key, KEY_SIZE);

	while (SMUserPointersFirst) {
		SMUser *tmp = SMUserPointersFirst;
		SMUserPointersFirst = SMUserPointersFirst->Next;
		delete tmp;
	}
}

bool ClientSession::InitSession()
{
	if (State != ClientStateUnconnected) {
		return false;
	}

	SetInputSizeLimit(1024);

	int64_t currentTime = GetUnixTime();

	CowBuffer<uint8_t> message(
		KEY_SIZE + sizeof(int64_t) + SIGNATURE_SIZE);

	memcpy(message.Pointer(), PublicKey, KEY_SIZE);
	memcpy(message.Pointer() + KEY_SIZE, &currentTime, sizeof(int64_t));

	Sign(
		message.Slice(0, KEY_SIZE + sizeof(int64_t)),
		SignaturePrivateKey,
		message.Pointer() + KEY_SIZE + sizeof(int64_t));

	Send(ApplyScrambler(message));

	State = ClientStateInitialWaitForServer;

	GenerateSessionKeys(
		PrivateKey,
		PublicKey,
		PeerPublicKey,
		currentTime,
		InES.Key,
		OutES.Key,
		true);

	InitNonce(OutES.Nonce);
	memset(InES.Nonce, 0, NONCE_SIZE);

	TimeState = currentTime;

	return true;
}

bool ClientSession::SendMessage(CowBuffer<uint8_t> message, void *userPointer)
{
	const uint32_t headerSize = KEY_SIZE * 2 +
		sizeof(int64_t) + sizeof(int32_t);

	if (!Connected()) {
		return false;
	}

	if (message.Size() <= headerSize) {
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

	int32_t command = SESSION_COMMAND_TEXT_MESSAGE;
	CowBuffer<uint8_t> data(message.Size() + sizeof(command));
	memcpy(data.Pointer(), &command, sizeof(command));

	memcpy(
		data.Pointer() + sizeof(command),
		message.Pointer(),
		message.Size());

	Send(Encrypt(data, OutES));
	return true;
}

bool ClientSession::RequestUserList()
{
	if (!Connected()) {
		return false;
	}

	int32_t command = SESSION_COMMAND_LIST_USERS;
	CowBuffer<uint8_t> request(sizeof(command));
	memcpy(request.Pointer(), &command, sizeof(command));
	Send(Encrypt(request, OutES));
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

	int32_t command = SESSION_COMMAND_KEEP_ALIVE;
	TimeState = GetUnixTime();

	CowBuffer<uint8_t> req(sizeof(command) + sizeof(TimeState));
	memcpy(req.Pointer(), &command, sizeof(command));
	memcpy(req.Pointer() + sizeof(command), &TimeState, sizeof(TimeState));

	Send(Encrypt(req, OutES));

	return true;
}

bool ClientSession::ProcessInitialWaitForServer()
{
	CowBuffer<uint8_t> cyphertext = Receive();

	CowBuffer<uint8_t> message = Decrypt(cyphertext, InES);

	if (message.Size() != KEY_SIZE + sizeof(int64_t)) {
		return false;
	}

	int res = crypto_verify32(message.Pointer(), PeerPublicKey);

	if (res) {
		return false;
	}

	int64_t value;
	memcpy(&value, message.Pointer() + KEY_SIZE, sizeof(value));

	if (value != TimeState + 1) {
		return false;
	}

	CowBuffer<uint8_t> response(sizeof(int64_t));
	memcpy(response.Pointer(), &value, sizeof(value));

	Send(Encrypt(response, OutES));

	State = ClientStateActiveSession;
	TimeState = 0;

	SetInputSizeLimit(1024 * 1024 * 1024);

	return true;
}

bool ClientSession::ProcessActiveSession()
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
		return ProcessSendMessage(plainText);
	} else if (command == SESSION_COMMAND_DELIVER_MESSAGE) {
		return ProcessDeliverMessage(plainText);
	} else if (command == SESSION_COMMAND_LIST_USERS) {
		return ProcessListUsers(plainText);
	}

	return false;
}

bool ClientSession::ProcessKeepAlive(CowBuffer<uint8_t> plainText)
{
	if (!TimeState) {
		return false;
	}

	if (plainText.Size() != sizeof(int32_t) + sizeof(int64_t)) {
		return false;
	}

	int64_t timestamp;
	memcpy(
		&timestamp,
		plainText.Pointer() + sizeof(int32_t),
		sizeof(timestamp));

	if (TimeState != timestamp) {
		return false;
	}

	TimeState = 0;

	return true;
}

bool ClientSession::ProcessSendMessage(CowBuffer<uint8_t> plainText)
{
	if (!SMUserPointersFirst) {
		return false;
	}

	if (plainText.Size() != sizeof(int32_t) * 2) {
		return false;
	}

	int32_t status;
	memcpy(&status, plainText.Pointer() + sizeof(int32_t), sizeof(status));

	void *userPointer = SMUserPointersFirst->Pointer;
	SMUser *tmp = SMUserPointersFirst;
	SMUserPointersFirst = SMUserPointersFirst->Next;
	delete tmp;

	if (!SMUserPointersFirst) {
		SMUserPointersLast = nullptr;
	}

	Processor->NotifyDelivery(userPointer, status);
	return true;
}

bool ClientSession::ProcessDeliverMessage(CowBuffer<uint8_t> plainText)
{
	if (plainText.Size() <= sizeof(int32_t)) {
		return false;
	}

	Processor->DeliverMessage(plainText.Slice(
		sizeof(int32_t),
		plainText.Size() - sizeof(int32_t)));
	return true;
}

bool ClientSession::ProcessListUsers(CowBuffer<uint8_t> plainText)
{
	if (plainText.Size() < sizeof(int32_t) * 2) {
		return false;
	}

	int32_t userCount;
	memcpy(
		&userCount,
		plainText.Pointer() + sizeof(int32_t),
		sizeof(userCount));

	const int entrySize = KEY_SIZE + 55;

	if (plainText.Size() != sizeof(int32_t) * 2 + entrySize * userCount) {
		return false;
	}

	for (int i = 0; i < userCount; i++) {
		const uint8_t *key = plainText.Pointer() +
			sizeof(int32_t) * 2 + i * entrySize;

		String name((const char*)plainText.Pointer() +
			sizeof(int32_t) * 2 +
			i * entrySize + KEY_SIZE);

		Processor->UpdateUserData(key, name);
	}

	return true;
}
