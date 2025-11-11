#ifndef _PROTOCOL_HPP
#define _PROTOCOL_HPP

#include "../Crypto/Crypto.hpp"
#include "../Common/CowBuffer.hpp"
#include "../Server/UserDB.hpp"
#include "../Server/MessagePipe.hpp"

struct Session
{
	struct Sequence
	{
		Sequence *Next;
		CowBuffer<uint8_t> Data;
	};

	Session();
	virtual ~Session();

	Session *Next;

	int64_t Time;
	int Socket;

	uint64_t ExpectedInput;
	CowBuffer<uint8_t> *Input;

	uint64_t RequiredOutput;
	CowBuffer<uint8_t> *Output;

	Sequence *InputSequence;
	Sequence *InputSequenceLast;

	Sequence *OutputSequence;
	Sequence *OutputSequenceLast;

	bool Read();
	bool Write();

	bool CanWrite()
	{
		return Output || OutputSequence;
	}

	bool CanReceive()
	{
		return InputSequence;
	}

	CowBuffer<uint8_t> Receive();
	void Send(CowBuffer<uint8_t> data);

	virtual bool Process();
	virtual bool TimePassed();
};

struct ServerSession : public Session
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
};

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

struct ControlSession : public Session
{
	UserDB *Users;
	bool *Work;

	bool Process() override;
	bool TimePassed() override;

	void SendResponse(int32_t value);

	void ProcessShutdownCommand();
	void ProcessAddUserCommand(CowBuffer<uint8_t> message);
	void ProcessRemoveUserCommand(CowBuffer<uint8_t> message);
	void ProcessUnknownCommand();
};

#endif
