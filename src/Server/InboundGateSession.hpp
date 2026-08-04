#ifndef _INBOUND_GATE_SESSION_HPP
#define _INBOUND_GATE_SESSION_HPP

#include "Config.hpp"
#include "RateLimiter.hpp"
#include "../Protocol/GateParser.hpp"
#include "../Common/EventDispatcher.hpp"
#include "../Common/StreamReader.hpp"
#include "../Common/StreamWriter.hpp"
#include "../Crypto/Crypto.hpp"

class InboundGateSessionStorage;

class InboundGateSession :
	public DescriptorEventProcessor,
	public TimeEventProcessor
{
public:
	InboundGateSession(
		int fd,
		uint32_t ipv4,
		InboundGateSessionStorage *storage,
		EventDispatcher *dispatcher,
		Config *config,
		RateLimiter *rateLimiter);
	~InboundGateSession();

	int GetDescriptor() override;
	bool RequestRead() override;
	bool RequestWrite() override;
	void ProcessRead() override;
	void ProcessWrite() override;

	void ProcessTimeEvent() override;

private:
	InboundGateSessionStorage *_storage;
	EventDispatcher *_dispatcher;
	Config *_config;
	RateLimiter *_rateLimiter;

	int _fd;
	uint32_t _ipv4;

	StreamReader *_reader;
	StreamWriter *_writer;

	uint8_t _outScramblerInit;
	uint8_t _inScramblerInit;

	Crypto::X25519::EncryptedStream _outES;
	Crypto::X25519::EncryptedStream _inES;

	String _peerName;

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
		HandshakeWaitVerificationResponse,
		OpenedSession
	};

	State _state;

	void SendInit();
	bool ProcessHandshakeSynSize(CowBuffer<uint8_t> buffer);
	bool ProcessHandshakeSyn(CowBuffer<uint8_t> buffer);
	GateHandshakeSyn::Data BuildUnsupportedProtocolVersion();
	GateHandshakeSyn::Data BuildUnsupportedEncryptionScheme();
	GateHandshakeSyn::Data BuildVerificationFailure();
	GateHandshakeSyn::Data BuildSyn();
	bool VerifyPeer(
		const CowBuffer<uint8_t> buffer,
		const CowBuffer<uint8_t> signature);

	bool ProcessHandshakeVerificationResponse(CowBuffer<uint8_t> buffer);
	bool ProcessSessionInput(CowBuffer<uint8_t> buffer);

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
