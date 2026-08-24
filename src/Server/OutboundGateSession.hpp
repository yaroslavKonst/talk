#ifndef _OUTBOUND_GATE_SESSION_HPP
#define _OUTBOUND_GATE_SESSION_HPP

#include "OutboundTaskBase.hpp"
#include "Config.hpp"
#include "GateSecurityModule.hpp"
#include "User.hpp"
#include "../Protocol/GateProtocol.hpp"
#include "../Message/Message.hpp"
#include "../Common/EventDispatcher.hpp"
#include "../Common/Resolver.hpp"
#include "../Common/StreamReader.hpp"
#include "../Common/StreamWriter.hpp"
#include "../Crypto/Crypto.hpp"

class TaskProcessChannelReportTarget
{
public:
	virtual ~TaskProcessChannelReportTarget()
	{ }

	virtual void ReportConnectionFailure() = 0;
	virtual void ReportRequestRateLimit() = 0;

	virtual bool ReportDeliveryStatus(
		bool success,
		int32_t errorCode) = 0;

	virtual CowBuffer<uint8_t> GetMessageForChannel() = 0;
};

class TaskProcessChannel : public OutboundTaskBase
{
public:
	String Source;
	String Destination;

	TaskProcessChannel();

	TaskProcessChannelReportTarget *ReportTarget;

	String GetConnectionDestination() override;
	void ReportConnectionFailure() override;
	void ReportRequestRateLimit() override;

	bool HasData() override;
	CowBuffer<uint8_t> GetData() override;
	bool ProcessData(const CowBuffer<uint8_t> buffer) override;

private:
	enum class State
	{
		Init,
		SentMessageHeader,
		SentMessageBody
	};

	State _state;

	bool _hasOutput;

	CowBuffer<uint8_t> _currentMessage;
	Message::X25519::HeaderPointToPoint _header;
};

class OutboundGateSessionStorage;

class OutboundGateSession :
	public DescriptorEventProcessor,
	public TimeEventProcessor,
	public ResolverUser
{
public:
	OutboundGateSession(
		EventDispatcher *dispatcher,
		Config *config,
		OutboundGateSessionStorage *storage,
		OutboundTaskBase *task);
	~OutboundGateSession();

	void CompleteInitialization();

	int GetDescriptor() override;
	bool RequestRead() override;
	bool RequestWrite() override;
	void ProcessRead() override;
	void ProcessWrite() override;

	void ProcessTimeEvent() override;

	void ResolveCompleted() override;

private:
	EventDispatcher *_dispatcher;
	Config *_config;
	OutboundGateSessionStorage *_storage;

	StreamReader *_reader;
	StreamWriter *_writer;

	int _fd;

	OutboundTaskBase *_task;

	enum class State
	{
		WaitingForDestinationResolve,
		WaitingForConnect,
		HandshakeWaitInit,
		HandshakeWaitSynSize,
		HandshakeWaitSyn,
		OpenedSession,
		SendAllAndShutdown
	};

	State _state;
	GateSecurityModule _securityModule;
	bool _requestedAResolve;

	uint8_t _inScramblerInit;
	uint8_t _outScramblerInit;

	Crypto::X25519::EncryptedStream _outES;
	Crypto::X25519::EncryptedStream _inES;

	Crypto::X25519::PrivateKeyContainer _ephemeralPrivateKey;
	Crypto::X25519::PublicKeyContainer _ephemeralPublicKey;
	CowBuffer<uint8_t> _salt1;
	CowBuffer<uint8_t> _salt2;

	// Connection.
	void StartConnection();
	void Resolve();
	void TryConnect();
	void ProcessConnect();
	void SetupHandshakeWaitInit();

	// Handshake processing.
	bool ProcessInit(const CowBuffer<uint8_t> buffer);
	void SendHandshakeSyn();
	bool ProcessSynSize(CowBuffer<uint8_t> buffer);
	bool ProcessSyn(CowBuffer<uint8_t> buffer);
	void SendVerificationStatus(bool success);

	// Session processing.
	GateProtocol *_protocol;
	bool ProcessSessionInput(CowBuffer<uint8_t> buffer);
	bool _expectChunkSize;

	// Logging.
	void OutboundGateLog(String message);
};

class OutboundGateSessionStorage
{
public:
	virtual ~OutboundGateSessionStorage()
	{ }

	virtual void MarkSessionForRemoval(OutboundGateSession *session) = 0;
};

#endif
