#include "Protocol.hpp"

#include <unistd.h>
#include <cstring>

#include "../ThirdParty/monocypher.h"

// Common.
void Session::Read()
{
	Time = GetUnixTime();

	if (!Input) {
		uint64_t size = 0;

		int res = read(Socket, &size, sizeof(size));

		if (res != sizeof(size)) {
			THROW("Invalid message size.");
		}

		ExpectedInput = size;
		Input = new CowBuffer<uint8_t>(size);
		return;
	}

	int64_t readBytes = read(
		Socket,
		Input->Pointer() + Input->Size() - ExpectedInput,
		ExpectedInput);

	if (readBytes <= 0) {
		THROW("Failed to receive data.");
	}

	ExpectedInput -= readBytes;
}

void Session::Write()
{
	Time = GetUnixTime();

	if (!Output) {
		return;
	}

	if (Output && !RequiredOutput)
	{
		uint64_t size = Output->Size();

		int res = write(Socket, &size, sizeof(size));

		if (res != sizeof(size)) {
			THROW("Failed to send message size.");
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
		THROW("Failed to send data.");
	}

	RequiredOutput -= writtenBytes;

	if (RequiredOutput == 0)
	{
		delete Output;
		Output = nullptr;
	}
}

// Server.
bool ServerSession::Process()
{
	if (!Input) {
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
	CowBuffer<uint8_t> message = *Input;
	delete Input;
	Input = nullptr;

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

	Output = new CowBuffer<uint8_t>();
	*Output = Encrypt(response, OutES);

	if (!Output->Size()) {
		delete Output;
		return false;
	}

	return true;
}

bool ServerSession::ProcessSecondSyn()
{
	CowBuffer<uint8_t> encryptedMessage = *Input;
	delete Input;
	Input = nullptr;

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

	return true;
}

bool ServerSession::ProcessActiveSession()
{
	return true;
}

// Client.
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
		SignatureKey,
		message.Pointer() + KEY_SIZE + sizeof(int64_t));

	Output = new CowBuffer<uint8_t>();
	*Output = message;

	State = ClientStateInitialWaitForServer;

	GenerateSessionKeys(
		PrivateKey,
		PublicKey,
		PeerPublicKey,
		currentTime,
		InES.Key,
		OutES.Key);

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
		return false; // TODO
	}

	return false;
}

bool ClientSession::ProcessInitialWaitForServer()
{
	CowBuffer<uint8_t> cyphertext = *Input;
	delete Input;
	Input = nullptr;

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

	Output = new CowBuffer<uint8_t>();
	*Output = Encrypt(response, OutES);

	State = ClientStateActiveSession;

	return true;
}
