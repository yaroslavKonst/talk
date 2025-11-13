#ifndef _CLIENT_SESSION_HPP
#define _CLIENT_SESSION_HPP

#include "Session.hpp"
#include "../Crypto/Crypto.hpp"

struct ClientSession : public Session
{
	~ClientSession();

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

	bool Process() override;
	bool ProcessInitialWaitForServer();
	bool ProcessActiveSession();

	bool TimePassed() override;
};

#endif
