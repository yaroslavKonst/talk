#ifndef _SERVER_SESSION_HPP
#define _SERVER_SESSION_HPP

#include "Session.hpp"
#include "../Server/UserDB.hpp"
#include "../Server/MessagePipe.hpp"
#include "../Crypto/Crypto.hpp"

struct ServerSession : public Session, public SendMessageHandler
{
	~ServerSession();

	enum ServerSessionState
	{
		ServerStateWaitFirstSyn = 0,
		ServerStateWaitSecondSyn = 1,
		ServerStateActiveSession = 2
	};

	UserDB *Users;
	MessagePipe *Pipe;

	ServerSessionState State;

	const uint8_t *SignatureKey;
	const uint8_t *PeerPublicKey;

	const uint8_t *PublicKey;
	const uint8_t *PrivateKey;

	EncryptedStream InES;
	EncryptedStream OutES;

	bool Process() override;
	bool ProcessFirstSyn();
	bool ProcessSecondSyn();
	bool ProcessActiveSession();

	bool TimePassed() override;

	bool ProcessKeepAlive(CowBuffer<uint8_t> plainText);
	bool ProcessTextMessage(CowBuffer<uint8_t> plainText);
	bool ProcessListUsers(CowBuffer<uint8_t> plainText);
	bool ProcessGetMessages(CowBuffer<uint8_t> plainText);

	void SendMessage(CowBuffer<uint8_t> message) override;

	// Voice.
	enum ServerSessionVoiceState
	{
		VoiceStateInactive = 0,
		VoiceStateWaitingForCallee = 1,
		VoiceStateRinging = 2,
		VoiceStateActive = 3
	};

	ServerSessionVoiceState VoiceState;
	SendMessageHandler *VoicePeer;
	bool InVoice() override;
	void SendVoiceFrame(CowBuffer<uint8_t> frame) override;
	void StartVoice(
		const uint8_t *peerKey,
		int64_t timestamp,
		SendMessageHandler *handler) override;
	void AcceptVoice() override;
	void DeclineVoice() override;
	void EndVoice() override;

	bool ProcessVoiceInit(CowBuffer<uint8_t> plainText);
	bool ProcessVoiceRequest(CowBuffer<uint8_t> plainText);
	bool ProcessVoiceEnd(CowBuffer<uint8_t> plainText);
	bool ProcessVoiceData(CowBuffer<uint8_t> plainText);
};

#endif
