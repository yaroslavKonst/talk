#ifndef _CLIENT_SESSION_HPP
#define _CLIENT_SESSION_HPP

#include "Session.hpp"
#include "../Crypto/Crypto.hpp"

class MessageProcessor
{
public:
	virtual ~MessageProcessor();

	// Text.
	virtual void NotifyDelivery(void *userPointer, int32_t status) = 0;
	virtual void DeliverMessage(CowBuffer<uint8_t> message) = 0;

	virtual void UpdateUserData(const uint8_t *key, String name) = 0;

	virtual int64_t GetLatestReceiveTimestamp() = 0;

	// Voice.
	virtual void VoiceRequest(const uint8_t *key, int64_t timestamp) = 0;
	virtual void VoiceInitResponse(int32_t code) = 0;
	virtual void VoiceEnd() = 0;
	virtual void ReceiveVoiceFrame(CowBuffer<uint8_t> frame) = 0;
};

struct ClientSession : public Session
{
	ClientSession();
	~ClientSession();

	bool Connected()
	{
		return Socket != -1;
	}

	void Disconnect();

	MessageProcessor *Processor;

	enum ClientSessionState
	{
		ClientStateUnconnected = 0,
		ClientStateInitialWaitForServer = 1,
		ClientStateActiveSession = 2
	};

	ClientSessionState State;
	int64_t TimeState;

	uint8_t SignaturePrivateKey[SIGNATURE_PRIVATE_KEY_SIZE];
	uint8_t SignaturePublicKey[SIGNATURE_PUBLIC_KEY_SIZE];
	uint8_t PeerPublicKey[KEY_SIZE];

	uint8_t PublicKey[KEY_SIZE];
	uint8_t PrivateKey[KEY_SIZE];

	EncryptedStream InES;
	EncryptedStream OutES;

	bool InitSession();
	bool SendMessage(const CowBuffer<uint8_t> message, void *userPointer);

	struct SMUser
	{
		void *Pointer;
		SMUser *Next;
	};

	SMUser *SMUserPointersFirst;
	SMUser *SMUserPointersLast;
	void ResetAllSent();

	bool RequestUserList();
	bool RequestNewMessages(int64_t timestamp);

	bool InitVoice(const uint8_t *key, int64_t timestamp);
	bool ResponseVoiceRequest(bool accept);
	bool EndVoice();
	bool SendVoiceFrame(const CowBuffer<uint8_t> frame);

	bool Process() override;
	bool ProcessInitialWaitForServer();
	bool ProcessActiveSession();

	bool TimePassed() override;

	bool ProcessKeepAlive(const CowBuffer<uint8_t> plainText);
	bool ProcessSendMessage(const CowBuffer<uint8_t> plainText);
	bool ProcessDeliverMessage(const CowBuffer<uint8_t> plainText);
	bool ProcessListUsers(const CowBuffer<uint8_t> plainText);

	bool ProcessVoiceInit(const CowBuffer<uint8_t> plainText);
	bool ProcessVoiceRequest(const CowBuffer<uint8_t> plainText);
	bool ProcessVoiceEnd(const CowBuffer<uint8_t> plainText);
	bool ProcessVoiceFrame(const CowBuffer<uint8_t> plainText);
};

#endif
