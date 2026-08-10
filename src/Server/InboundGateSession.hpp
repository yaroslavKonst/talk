#ifndef _INBOUND_GATE_SESSION_HPP
#define _INBOUND_GATE_SESSION_HPP

#include "Config.hpp"
#include "RateLimiter.hpp"
#include "UserDB.hpp"
#include "GateSecurityModule.hpp"
#include "../Protocol/GateParser.hpp"
#include "../Protocol/GateProtocol.hpp"
#include "../Common/EventDispatcher.hpp"
#include "../Common/Resolver.hpp"
#include "../Common/StreamReader.hpp"
#include "../Common/StreamWriter.hpp"
#include "../Crypto/Crypto.hpp"

class InboundTaskBase
{
public:
	static InboundTaskBase *GetTask(
		int32_t command,
		UserDB *users,
		GateSecurityModule *secMod);

	virtual ~InboundTaskBase()
	{ }

	// Has data to write.
	virtual bool HasData() = 0;
	// Get data. Returned empty buffer must close the session.
	virtual CowBuffer<uint8_t> GetData() = 0;
	// Process data. Returned false must close the session.
	virtual bool ProcessData(const CowBuffer<uint8_t> buffer) = 0;

protected:
	InboundTaskBase()
	{ }
};

class InboundTaskReceiveMessage : public InboundTaskBase
{
public:
	InboundTaskReceiveMessage(
		UserDB *users,
		GateSecurityModule *secMod);

	bool HasData() override;
	CowBuffer<uint8_t> GetData() override;
	bool ProcessData(const CowBuffer<uint8_t> buffer) override;

private:
	UserDB *_users;
	GateSecurityModule *_securityModule;

	enum class State
	{
		WaitForHeader,
		WaitForBody
	};

	State _state;

	Message::X25519::HeaderPointToPoint _header;
	CowBuffer<uint8_t> _receivedPart;
	String _userName;
	bool _silentBlock;

	CowBuffer<uint8_t> _response;

	bool ProcessWaitHeader(const CowBuffer<uint8_t> buffer);
	bool ProcessWaitBody(const CowBuffer<uint8_t> buffer);
	void SendVerificationCode(int32_t code);
};

class InboundGateSessionStorage;

class InboundGateSession :
	public DescriptorEventProcessor,
	public TimeEventProcessor,
	public ResolverUser
{
public:
	InboundGateSession(
		int fd,
		uint32_t ipv4,
		InboundGateSessionStorage *storage,
		EventDispatcher *dispatcher,
		UserDB *users,
		Config *config,
		RateLimiter *rateLimiter);
	~InboundGateSession();

	int GetDescriptor() override;
	bool RequestRead() override;
	bool RequestWrite() override;
	void ProcessRead() override;
	void ProcessWrite() override;

	void ProcessTimeEvent() override;

	void ResolveCompleted() override;

private:
	InboundGateSessionStorage *_storage;
	EventDispatcher *_dispatcher;
	UserDB *_users;
	Config *_config;
	RateLimiter *_rateLimiter;

	int _fd;
	uint32_t _ipv4;

	StreamReader *_reader;
	StreamWriter *_writer;

	GateProtocol *_protocol;
	bool _expectedChunkSize;

	uint8_t _outScramblerInit;
	uint8_t _inScramblerInit;

	Crypto::X25519::EncryptedStream _outES;
	Crypto::X25519::EncryptedStream _inES;

	GateSecurityModule _securityModule;

	Crypto::X25519::PrivateKeyContainer _ephemeralPrivateKey;
	Crypto::X25519::PublicKeyContainer _ephemeralPublicKey;
	Crypto::X25519::PublicKeyContainer _peerPublicKey;

	CowBuffer<uint8_t> _salt1;
	CowBuffer<uint8_t> _salt2;

	enum class State
	{
		WriteAllAndExit,
		HandshakeWaitSynSize,
		HandshakeWaitSyn,
		HandshakeWaitPeerResolving,
		HandshakeWaitVerificationResponse,
		OpenedSession
	};

	State _state;

	void SendInit();
	bool ProcessHandshakeSynSize(CowBuffer<uint8_t> buffer);
	bool ProcessHandshakeSyn(CowBuffer<uint8_t> buffer);
	void SendUnsupportedProtocolVersion();
	void SendUnsupportedEncryptionScheme();
	void SendVerificationFailure();
	void SendSyn();
	void VerifyPeer();
	CowBuffer<uint8_t> _peerSynBuffer;
	CowBuffer<uint8_t> _peerSynSignature;

	bool ProcessHandshakeVerificationResponse(CowBuffer<uint8_t> buffer);
	bool ProcessSessionInput(CowBuffer<uint8_t> buffer);

	InboundTaskBase *_task;

	void CreateSizePrefixAndSend(CowBuffer<uint8_t> buffer);
	void InboundGateLog(String message);
};

class InboundGateSessionStorage
{
public:
	virtual ~InboundGateSessionStorage()
	{ }

	virtual void MarkSessionForRemoval(InboundGateSession *session) = 0;
};

#endif
