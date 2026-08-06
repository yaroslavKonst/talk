#ifndef _OUTBOUND_GATE_SESSION_HPP
#define _OUTBOUND_GATE_SESSION_HPP

#include "Config.hpp"
#include "../Protocol/GateProtocol.hpp"
#include "../Message/Message.hpp"
#include "../Common/EventDispatcher.hpp"
#include "../Common/Resolver.hpp"
#include "../Common/StreamReader.hpp"
#include "../Common/StreamWriter.hpp"
#include "../Crypto/Crypto.hpp"

enum class TaskType
{
	ProcessChannel
};

enum class TaskResult
{
	RequestLimitOverflow,
	ConnectionFailure
};

class TaskBase
{
public:
	TaskType Type;

	TaskBase();
	virtual ~TaskBase()
	{ }

	virtual String GetConnectionDestination() = 0;
	virtual void ReportConnectionFailure() = 0;
	virtual void ReportRequestRateLimit() = 0;

	virtual bool HasData() = 0;
	virtual CowBuffer<uint8_t> GetData() = 0;
	virtual bool ProcessData(const CowBuffer<uint8_t> buffer) = 0;

protected:
	bool MustReportFailure();
	void MarkFailureReport();

private:
	bool _reportedFailure;
};

class TaskProcessChannelReportTarget
{
public:
	virtual ~TaskProcessChannelReportTarget()
	{ }

	virtual void ReportConnectionFailure() = 0;
	virtual void ReportRequestRateLimit() = 0;

	virtual bool ReportDeliverySuccess() = 0;
	virtual bool ReportDeliveryFailure(int32_t reason) = 0;

	virtual CowBuffer<uint8_t> GetMessageForChannel(
		String source,
		String destination) = 0;
};

class TaskProcessChannel : public TaskBase
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
		TaskBase *task);
	~OutboundGateSession();

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
	uint32_t _ipv4;

	TaskBase *_task;

	enum class State
	{
		WaitingForDestinationNameResolve,
		WaitingForConnect,
		HandshakeWaitInit,
		HandshakeWaitSynSize,
		HandshakeWaitSyn,
		OpenedSession,
		SendAllAndShutdown
	};

	State _state;
	Resolver _resolver;
	struct addrinfo *_addrinfo;

	uint8_t _inScramblerInit;
	uint8_t _outScramblerInit;

	Crypto::X25519::EncryptedStream _outES;
	Crypto::X25519::EncryptedStream _inES;

	String _peerName;

	Crypto::X25519::PrivateKeyContainer _ephemeralPrivateKey;
	Crypto::X25519::PublicKeyContainer _ephemeralPublicKey;
	CowBuffer<uint8_t> _salt1;
	CowBuffer<uint8_t> _salt2;

	// Connection.
	void StartConnection();
	void TryConnect();
	void ProcessConnect();
	void SetupHandshakeWaitInit();

	// Handshake processing.
	bool ProcessInit(const CowBuffer<uint8_t> buffer);
	void SendHandshakeSyn();
	bool ProcessSynSize(CowBuffer<uint8_t> buffer);
	bool ProcessSyn(CowBuffer<uint8_t> buffer);
	bool VerifyPeer(
		const CowBuffer<uint8_t> syn,
		const CowBuffer<uint8_t> signature);
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
