#ifndef _CLIENT_SESSION_HPP
#define _CLIENT_SESSION_HPP

#include "Session.hpp"
#include "../Client/Processors.hpp"
#include "../Client/Root.hpp"
#include "../Crypto/Crypto.hpp"

struct ClientSession : public Session, public NetworkEventProcessor
{
	ClientSession(Root *root);
	~ClientSession();

	uint8_t *GetPrivateKey() override
	{
		return PrivateKey;
	}

	uint8_t *GetPublicKey() override
	{
		return PublicKey;
	}

	uint8_t *GetSignaturePrivateKey() override
	{
		return SignaturePrivateKey;
	}

	uint8_t *GetSignaturePublicKey() override
	{
		return SignaturePublicKey;
	}

	uint8_t *GetPeerPublicKey() override
	{
		return PeerPublicKey;
	}

	int &GetSocket() override
	{
		return Socket;
	}

	bool Connected() override
	{
		return Socket != -1;
	}

	bool ConnectedActive() override
	{
		return Socket != -1 && State == ClientStateActiveSession;
	}

	void Disconnect() override;

	Root *ClientRoot;

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

	struct Stream
	{
		EncryptedStream InES;
		EncryptedStream OutES;
	};

	Stream Streams[StreamCount];

	bool InitSession() override;
	bool SendMessage(const CowBuffer<uint8_t> message) override;

	struct SMUser
	{
		void *Pointer;
		SMUser *Next;
	};

	SMUser *SMUserPointersFirst;
	SMUser *SMUserPointersLast;
	void ResetAllSent();

	bool RequestUserList() override;
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
