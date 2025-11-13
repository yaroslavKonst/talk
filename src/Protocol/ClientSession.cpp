#include "ClientSession.hpp"

#include "ActiveSession.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/Exception.hpp"

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
	} else if (command == SESSION_COMMAND_TEXT_MESSAGE) {
		return true;
	}

	return false;
}
