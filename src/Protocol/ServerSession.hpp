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

	void SendMessage(CowBuffer<uint8_t> message) override;
};

#endif
