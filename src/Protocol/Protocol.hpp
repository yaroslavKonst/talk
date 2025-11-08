#ifndef _PROTOCOL_HPP
#define _PROTOCOL_HPP

#include "../Crypto/Crypto.hpp"
#include "../Common/CowBuffer.hpp"
#include "../Server/UserDB.hpp"

struct Session
{
	int64_t Time;
	int Socket;

	uint64_t ExpectedInput;
	CowBuffer<uint8_t> *Input;

	uint64_t RequiredOutput;
	CowBuffer<uint8_t> *Output;

	void Read();
	void Write();
};

struct ServerSession : public Session
{
	enum ServerSessionState
	{
		ServerStateWaitFirstSyn = 0,
		ServerStateWaitSecondSyn = 1,
		ServerStateActiveSession = 2
	};

	ServerSession *Next;

	UserDB *Users;
	//MessagePipe *Pipe;

	ServerSessionState State;

	const uint8_t *SignatureKey;
	const uint8_t *PeerPublicKey;

	const uint8_t *PublicKey;
	const uint8_t *PrivateKey;

	EncryptedStream InES;
	EncryptedStream OutES;

	bool Process();
	bool ProcessFirstSyn();
	bool ProcessSecondSyn();
	bool ProcessActiveSession();

	bool TimePassed();
};

struct ClientSession : public Session
{
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

	bool Process();
	bool ProcessInitialWaitForServer();

	bool TimePassed();
};

#endif
