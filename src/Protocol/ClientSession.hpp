#ifndef _CLIENT_SESSION_HPP
#define _CLIENT_SESSION_HPP

#include "Session.hpp"
#include "../Crypto/Crypto.hpp"

class MessageProcessor
{
public:
	virtual ~MessageProcessor();

	virtual void NotifyDelivery(CowBuffer<uint8_t> header) = 0;
	virtual void DeliverMessage(CowBuffer<uint8_t> message) = 0;
};

struct ClientSession : public Session
{
	~ClientSession();

	bool Connected()
	{
		return Socket != -1;
	}

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
	bool SendMessage(CowBuffer<uint8_t> message);
	CowBuffer<uint8_t> SMHeader;

	bool Process() override;
	bool ProcessInitialWaitForServer();
	bool ProcessActiveSession();

	bool TimePassed() override;

	bool ProcessKeepAlive(CowBuffer<uint8_t> plainText);
	bool ProcessSendMessage(CowBuffer<uint8_t> plainText);
	bool ProcessDeliverMessage(CowBuffer<uint8_t> plainText);
};

#endif
