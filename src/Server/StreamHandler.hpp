#ifndef _STREAM_HANDLER_HPP
#define _STREAM_HANDLER_HPP

#include "ServerSession.hpp"
#include "OutboundTaskBase.hpp"
#include "../Protocol/StreamParser.hpp"

class StreamHandlerUser;

class StreamHandler :
	public StreamProcessorBase,
	public OutboundTaskBase
{
public:
	StreamHandler(
		String userName,
		Crypto::X25519::PublicKeyContainer *publicKey,
		Config *config,
		StreamHandlerUser *user);
	~StreamHandler();

	bool IsIdle();

	// Gate methods.
	bool MustBeDeleted() override
	{
		return false;
	}

	virtual int64_t GetTimeoutInterval() override
	{
		return 30000;
	}

	virtual bool TimeoutAction() override;

	String GetConnectionDestination() override;
	void ReportConnectionFailure() override;
	void ReportRequestRateLimit() override;
	void NotifyGateSessionClosed() override;

	bool HasData() override;
	CowBuffer<uint8_t> GetData() override;
	bool ProcessData(const CowBuffer<uint8_t> buffer) override;

	// Stream methods.
	void EndStream(ServerSession *session) override;

	// User session methods.
	bool ProcessUserStreamInit(
		const CowBuffer<uint8_t> initRequest,
		ServerSession *userSession) override;
	int32_t ProcessGateStreamInit(const CowBuffer<uint8_t> initRequest);
	void NotifyUserSessionClosed(ServerSession *session) override;
	bool ProcessUserStreamResponse(
		const CowBuffer<uint8_t> initResponse,
		ServerSession *session) override;

	void CheckNewSession(ServerSession *session) override;

private:
	String _userName;
	Crypto::X25519::PublicKeyContainer *_publicKey;
	Config *_config;
	StreamHandlerUser *_user;

	CowBuffer<uint8_t> _outboundMessage;

	enum class State
	{
		Closed,

		// Outbound states.
		ReadyToSendGateInit,
		WaitingForGateInitResponse,
		WaitingForGatePeerResponse,

		// Inbound states.
		WaitingForUserAnswer,

		// Opened session state.
		OpenedSession
	};

	State _state;

	ServerSession *_userSession;
	String _destination;
	CowBuffer<uint8_t> _initRequest;
	bool _hasGateSession;

	bool ProcessGateKeepAlive(const CowBuffer<uint8_t> buffer);
	void SendGateKeepAlive();
	bool _sentGateKeepAlive;

	bool ProcessGateStreamEnd(const CowBuffer<uint8_t> buffer);
	void SendGateStreamEnd();

	bool ProcessOpenedSession(const CowBuffer<uint8_t> buffer);

	struct QueueItem
	{
		QueueItem *Next;
		CowBuffer<uint8_t> Data;
	};

	QueueItem *_queueFirst;
	QueueItem *_queueLast;

	void PushQueue(const CowBuffer<uint8_t> buffer);

	void StreamLog(String message);
};

class StreamHandlerUser
{
public:
	virtual ~StreamHandlerUser()
	{ }

	virtual void StartStreamGateSession() = 0;
	virtual int32_t CheckInboundCall(
		const StreamHandshake::InitRequest &request) = 0;
	virtual bool BroadcastStreamRequest(
		const CowBuffer<uint8_t> initRequest) = 0;
	virtual void BroadcastStreamEnd(ServerSession *exception) = 0;
};

#endif
