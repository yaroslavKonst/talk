#include "ServerSession.hpp"

#include "ActiveSession.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/MessageContainer.hpp"

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
	message = RemoveScrambler(message);

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

	if (command == SESSION_COMMAND_KEEP_ALIVE) {
		if (plainText.Size() != sizeof(command) + sizeof(int64_t)) {
			return false;
		}

		Send(Encrypt(plainText, OutES));
		return true;
	} else if (command == SESSION_COMMAND_TEXT_MESSAGE) {
		int32_t status = SESSION_RESPONSE_OK;

		CowBuffer<uint8_t> response(sizeof(int32_t) * 2);
		memcpy(
			response.Pointer(),
			plainText.Pointer(),
			sizeof(int32_t));

		if (plainText.Size() <= sizeof(command) + KEY_SIZE * 2 +
			sizeof(int64_t))
		{
			status = SESSION_RESPONSE_ERROR_MESSAGE_TOO_SHORT;
		} else {
			CowBuffer<uint8_t> message = plainText.Slice(
				sizeof(command),
				plainText.Size() - sizeof(command));

			if (!Users->HasUser(message.Pointer())) {
				status = SESSION_RESPONSE_ERROR_INVALID_USER;
			}

			if (!Users->HasUser(message.Pointer() + KEY_SIZE)) {
				status = SESSION_RESPONSE_ERROR_INVALID_USER;
			}

			if (status == SESSION_RESPONSE_OK) {
				MessageContainer container1(
					message.Pointer(),
					message.Pointer() + KEY_SIZE);
				container1.AddMessage(message);

				MessageContainer container2(
					message.Pointer() + KEY_SIZE,
					message.Pointer());
				container2.AddMessage(message);

				Pipe->SendMessage(message);
			}
		}

		memcpy(
			response.Pointer() + sizeof(command),
			&status,
			sizeof(status));

		Send(Encrypt(response, OutES));
		return true;
	}

	return false;
}
