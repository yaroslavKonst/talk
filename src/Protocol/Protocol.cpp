#include "Protocol.hpp"

#include <unistd.h>
#include <sys/socket.h>
#include <cstring>

#include "../ServerCtl/SocketName.hpp"
#include "../ThirdParty/monocypher.h"
#include "../Common/Hex.hpp"
#include "../Common/Debug.hpp"

// Base.
Session::Session()
{
	Next = nullptr;

	Time = GetUnixTime();
	Socket = -1;

	Input = nullptr;
	ExpectedInput = 0;

	Output = nullptr;
	RequiredOutput = 0;

	InputSequence = nullptr;
	InputSequenceLast = nullptr;

	OutputSequence = nullptr;
	OutputSequenceLast = nullptr;
}

Session::~Session()
{
	while (InputSequence) {
		Sequence *elem = InputSequence;
		InputSequence = InputSequence->Next;
		delete elem;
	}

	while (OutputSequence) {
		Sequence *elem = OutputSequence;
		OutputSequence = OutputSequence->Next;
		delete elem;
	}

	if (Input) {
		delete Input;
	}

	if (Output) {
		delete Output;
	}

	if (Socket != -1) {
		shutdown(Socket, SHUT_RDWR);
		close(Socket);
	}
}

bool Session::Read()
{
	Time = GetUnixTime();

	if (!Input) {
		uint64_t size = 0;

		int res = read(Socket, &size, sizeof(size));

		if (res != sizeof(size)) {
			return false;
		}

		ExpectedInput = size;
		Input = new CowBuffer<uint8_t>(size);
		return true;
	}

	int64_t readBytes = read(
		Socket,
		Input->Pointer() + Input->Size() - ExpectedInput,
		ExpectedInput);

	if (readBytes <= 0) {
		return false;
	}

	ExpectedInput -= readBytes;

	if (ExpectedInput == 0) {
		Sequence *elem = new Sequence;
		elem->Next = nullptr;
		elem->Data = *Input;
		delete Input;
		Input = nullptr;

		if (InputSequence) {
			InputSequenceLast->Next = elem;
			InputSequenceLast = elem;
		} else {
			InputSequence = elem;
			InputSequenceLast = elem;
		}
	}

	return true;
}

bool Session::Write()
{
	Time = GetUnixTime();

	if (!Output && !OutputSequence) {
		return true;
	}

	if (!Output) {
		Output = new CowBuffer<uint8_t>();

		Sequence *elem = OutputSequence;
		*Output = elem->Data;

		OutputSequence = elem->Next;

		if (!OutputSequence) {
			OutputSequenceLast = nullptr;
		}

		delete elem;
	}

	if (Output && !RequiredOutput)
	{
		uint64_t size = Output->Size();

		int res = write(Socket, &size, sizeof(size));

		if (res != sizeof(size)) {
			return false;
		}

		RequiredOutput = size;
	}

	uint64_t limit = 1024;

	if (RequiredOutput < limit) {
		limit = RequiredOutput;
	}

	int64_t writtenBytes = write(
		Socket,
		Output->Pointer() + Output->Size() - RequiredOutput,
		limit);

	if (writtenBytes <= 0) {
		return false;
	}

	RequiredOutput -= writtenBytes;

	if (RequiredOutput == 0)
	{
		delete Output;
		Output = nullptr;
	}

	return true;
}

CowBuffer<uint8_t> Session::Receive()
{
	if (!InputSequence) {
		return CowBuffer<uint8_t>();
	}

	Sequence *elem = InputSequence;

	CowBuffer<uint8_t> data = elem->Data;

	InputSequence = elem->Next;

	if (!InputSequence) {
		InputSequenceLast = nullptr;
	}

	delete elem;

	return data;
}

void Session::Send(CowBuffer<uint8_t> data)
{
	Sequence *elem = new Sequence;
	elem->Next = nullptr;
	elem->Data = data;

	if (OutputSequence) {
		OutputSequenceLast->Next = elem;
		OutputSequenceLast = elem;
	} else {
		OutputSequence = elem;
		OutputSequenceLast = elem;
	}
}

// Server.
ServerSession::~ServerSession()
{
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

	if (message.Size() != KEY_SIZE + sizeof(int64_t) + SIGNATURE_SIZE) {
		return false;
	}

	if (!Users->HasUser(message.Pointer())) {
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

	if (command == 1) {
		if (plainText.Size() != sizeof(command) + sizeof(int64_t)) {
			return false;
		}

		Send(Encrypt(plainText, OutES));
		return true;
	}

	return false;
}

// Client.
ClientSession::~ClientSession()
{
	crypto_wipe(SignaturePrivateKey, SIGNATURE_PRIVATE_KEY_SIZE);
	crypto_wipe(SignaturePublicKey, SIGNATURE_PUBLIC_KEY_SIZE);
	crypto_wipe(PeerPublicKey, KEY_SIZE);
	crypto_wipe(PublicKey, KEY_SIZE);
	crypto_wipe(PrivateKey, KEY_SIZE);
	crypto_wipe(InES.Key, KEY_SIZE);
	crypto_wipe(OutES.Key, KEY_SIZE);
}

bool ClientSession::InitSession()
{
	if (State != ClientStateUnconnected) {
		THROW("Session is active.");
	}

	int64_t currentTime = GetUnixTime();

	CowBuffer<uint8_t> message(
		KEY_SIZE + sizeof(int64_t) + SIGNATURE_SIZE);

	memcpy(message.Pointer(), PublicKey, KEY_SIZE);
	memcpy(message.Pointer() + KEY_SIZE, &currentTime, sizeof(int64_t));

	Sign(
		message.Slice(0, KEY_SIZE + sizeof(int64_t)),
		SignaturePrivateKey,
		message.Pointer() + KEY_SIZE + sizeof(int64_t));

	Send(message);

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

	int32_t command = 1;
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

	if (command == 1) {
		if (!TimeState) {
			return false;
		}

		if (plainText.Size() != sizeof(int32_t) + sizeof(int64_t)) {
			return false;
		}

		int64_t timestamp;
		memcpy(
			&timestamp,
			plainText.Pointer() + sizeof(command),
			sizeof(timestamp));

		if (TimeState != timestamp) {
			return false;
		}

		TimeState = 0;

		return true;
	}

	return false;
}

// Control.
bool ControlSession::TimePassed()
{
	int64_t t = GetUnixTime();

	if (t - Time > 10) {
		return false;
	}

	return true;
}

// Command structure.
// | Command id (int32) |
// |        stop        |
// |      add user      | key | signature | name size (int32) | name |
// |    remove user     | key |
bool ControlSession::Process()
{
	CowBuffer<uint8_t> message = Receive();

	if (message.Size() < sizeof(int32_t)) {
		return false;
	}

	int32_t command;
	memcpy(&command, message.Pointer(), sizeof(int32_t));

	switch (command) {
	case COMMAND_SHUTDOWN:
		ProcessShutdownCommand();
		break;
	case COMMAND_ADD_USER:
		ProcessAddUserCommand(message);
		break;
	case COMMAND_REMOVE_USER:
		ProcessRemoveUserCommand(message);
		break;
	default:
		ProcessUnknownCommand();
		break;
	}

	return true;
}

void ControlSession::SendResponse(int32_t value)
{
	CowBuffer<uint8_t> message(sizeof(value));
	memcpy(message.Pointer(), &value, sizeof(value));
	Send(message);
}

// |        stop        |
void ControlSession::ProcessShutdownCommand()
{
	*Work = false;
}

// |      add user      | key | signature | name size (int32) | name |
void ControlSession::ProcessAddUserCommand(CowBuffer<uint8_t> message)
{
	uint32_t minMessageLength =
		sizeof(int32_t) +
		KEY_SIZE +
		SIGNATURE_PUBLIC_KEY_SIZE +
		sizeof(int32_t);

	if (message.Size() < minMessageLength) {
		SendResponse(ERROR_TOO_SHORT);
		return;
	}

	const uint8_t *key = message.Pointer() + sizeof(int32_t);
	const uint8_t *signature = message.Pointer() + sizeof(int32_t) +
		KEY_SIZE;

	int32_t nameSize;
	memcpy(
		&nameSize,
		message.Pointer() + sizeof(int32_t) + KEY_SIZE +
		SIGNATURE_PUBLIC_KEY_SIZE,
		sizeof(int32_t));

	String name;

	if (nameSize) {
		bool messageCorrect =
			message.Size() >=
			sizeof(int32_t) * 2 + KEY_SIZE +
			SIGNATURE_PUBLIC_KEY_SIZE + nameSize;

		if (!messageCorrect) {
			SendResponse(ERROR_TOO_SHORT);
			return;
		}

		int indexBase = sizeof(int32_t) * 2 + KEY_SIZE +
			SIGNATURE_PUBLIC_KEY_SIZE;

		for (int i = 0; i < nameSize; i++) {
			name += message[i + indexBase];
		}
	}

	Users->AddUser(key, signature, GetUnixTime());

	SendResponse(OK);
}

void ControlSession::ProcessRemoveUserCommand(CowBuffer<uint8_t> message)
{
}

void ControlSession::ProcessUnknownCommand()
{
	SendResponse(ERROR_UNKNOWN_COMMAND);
}
