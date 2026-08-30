#include "StreamHandler.hpp"

#include "../Protocol/StreamParser.hpp"
#include "../Protocol/GateParser.hpp"
#include "../Protocol/CommonParserConstants.hpp"
#include "../Common/Endianness.hpp"
#include "../Common/Log.hpp"

StreamHandler::StreamHandler(
	String name,
	Crypto::X25519::PublicKeyContainer *publicKey,
	Config *config,
	StreamHandlerUser *user)
{
	_userName = name;
	_publicKey = publicKey;
	_config = config;
	_user = user;

	_state = State::Closed;
	_userSession = nullptr;
	_hasGateSession = false;

	_sentGateKeepAlive = false;

	_queueFirst = nullptr;
	_queueLast = nullptr;
}

StreamHandler::~StreamHandler()
{
	EndStream(nullptr);
}

bool StreamHandler::IsIdle()
{
	return _state == State::Closed && !_hasGateSession;
}

bool StreamHandler::TimeoutAction()
{
	StreamLog("Timeout.");

	bool allowedState =
		_state == State::WaitingForGatePeerResponse ||
		_state == State::OpenedSession;

	if (!allowedState) {
		StreamLog("Wrong state.");
		return false;
	}

	if (_sentGateKeepAlive) {
		StreamLog("Keep alive already sent.");
		return false;
	}

	StreamLog("Sending keep alive.");
	SendGateKeepAlive();
	_sentGateKeepAlive = true;
	return true;
}

String StreamHandler::GetConnectionDestination()
{
	return _destination;
}

void StreamHandler::ReportConnectionFailure()
{
	StreamLog("Reported connection failure.");

	bool validState =
		_state == State::ReadyToSendGateInit ||
		_state == State::WaitingForGateInitResponse;

	if (validState) {
		if (_userSession) {
			_userSession->SendStreamInit(
				STREAM_INIT_RESPONSE_SERVER_OFFLINE);
		}

		_userSession = nullptr;
	}

	EndStream(nullptr);
}

void StreamHandler::ReportRequestRateLimit()
{
	StreamLog("Reported request rate limit overflow.");

	bool validState =
		_state == State::ReadyToSendGateInit ||
		_state == State::WaitingForGateInitResponse;

	if (validState) {
		if (_userSession) {
			_userSession->SendStreamInit(
				STREAM_INIT_RESPONSE_ERROR);
		}

		_userSession = nullptr;
	}

	EndStream(nullptr);
}

void StreamHandler::NotifyGateSessionClosed()
{
	StreamLog("Gate closed.");

	_hasGateSession = false;
	EndStream(nullptr);
}

bool StreamHandler::HasData()
{
	if (_outboundMessage.Size()) {
		return true;
	}

	if (_state == State::Closed) {
		return true;
	}

	if (_state == State::ReadyToSendGateInit) {
		return true;
	}

	return _queueFirst;
}

CowBuffer<uint8_t> StreamHandler::GetData()
{
	if (_outboundMessage.Size()) {
		CowBuffer<uint8_t> buffer = _outboundMessage;
		_outboundMessage = CowBuffer<uint8_t>();
		return buffer;
	}

	if (_state == State::Closed) {
		StreamLog("GetData on closed state.");
		return CowBuffer<uint8_t>();
	}

	if (_state == State::ReadyToSendGateInit) {
		StreamLog("Sent init request.");
		_state = State::WaitingForGateInitResponse;

		GateCommandStream::InitRequest gateRequest;
		gateRequest.Request = _initRequest;
		return GateCommandStream::BuildInitRequest(gateRequest);
	}

	if (!_queueFirst) {
		StreamLog("Empty queue.");
		return CowBuffer<uint8_t>();
	}

	CowBuffer<uint8_t> data = _queueFirst->Data;

	QueueItem *tmp = _queueFirst;
	_queueFirst = _queueFirst->Next;
	delete tmp;

	if (!_queueFirst) {
		_queueLast = nullptr;
	}

	return data;
}

bool StreamHandler::ProcessData(const CowBuffer<uint8_t> buffer)
{
	bool inInvalidState =
		_state == State::Closed ||
		_state == State::ReadyToSendGateInit;

	if (inInvalidState) {
		StreamLog("Process in invalid state.");
		return false;
	}

	if (buffer.Size() > CommonParserConstants::SmallDatagramSize) {
		return false;
	}

	if (_state == State::WaitingForGateInitResponse) {
		StreamLog("Processing init response from gate.");

		GateCommandStream::InitResponse response;
		bool parseResult = GateCommandStream::ParseInitResponse(
			buffer,
			response);

		if (!parseResult) {
			EndStream(nullptr);
			return false;
		}

		if (_userSession) {
			_userSession->SendStreamInit(response.Code);
		}

		if (response.Code != STREAM_INIT_RESPONSE_WAITING_FOR_ANSWER) {
			EndStream(nullptr);
			return false;
		}

		_state = State::WaitingForGatePeerResponse;
		return true;
	}

	if (_state == State::WaitingForGatePeerResponse) {
		StreamLog("Processing peer response from gate.");

		if (ProcessGateKeepAlive(buffer)) {
			return true;
		}

		if (_userSession) {
			_userSession->SendStreamResponse(buffer);
		}

		// Response is encrypted, it can not be checked here.
		// The client and the peer server are expected to close
		// the stream session in case of declined call.
		_state = State::OpenedSession;
		return true;
	}

	if (_state == State::WaitingForUserAnswer) {
		StreamLog("Processing keep alive.");
		return ProcessGateKeepAlive(buffer);
	}

	return ProcessOpenedSession(buffer);
}

void StreamHandler::EndStream(ServerSession *session)
{
	if (_userSession && session && session != _userSession) {
		return;
	}

	StreamLog("End stream.");

	if (_state == State::WaitingForUserAnswer) {
		_user->BroadcastStreamEnd(nullptr);
	} else if (_userSession) {
		_userSession->SendStreamEnd();
	}

	_userSession = nullptr;
	_state = State::Closed;
	_destination = "";
	_initRequest = CowBuffer<uint8_t>();

	_sentGateKeepAlive = false;

	while (_queueFirst) {
		QueueItem *tmp = _queueFirst;
		_queueFirst = _queueFirst->Next;
		delete tmp;
	}

	_queueLast = nullptr;
}

bool StreamHandler::ProcessUserStreamInit(
	const CowBuffer<uint8_t> initRequest,
	ServerSession *session)
{
	StreamLog("ProcessUserStreamInit.");

	if (session == _userSession) {
		EndStream(nullptr);
		StreamLog("Duplicate.");
		return false;
	}

	StreamHandshake::InitRequest request;
	bool parseResult = StreamHandshake::ParseInitRequest(
		initRequest,
		request);

	if (!parseResult) {
		StreamLog("Parse failure.");
		return false;
	}

	if (!Message::VerifyFullUserName(request.Source)) {
		StreamLog("Invalid source.");
		return false;
	}

	if (!Message::VerifyFullUserName(request.Destination)) {
		StreamLog("Invalid destination.");
		return false;
	}

	if (_state != State::Closed || _hasGateSession) {
		session->SendStreamInit(
			STREAM_INIT_RESPONSE_YOU_ARE_IN_CALL);
		StreamLog("Call already active.");
		return true;
	}

	if (request.Source != _userName + "@" + _config->GetHostName()) {
		StreamLog("Wrong source name.");
		return false;
	}

	if (crypto_verify32(request.SourceKey.Key, _publicKey->Key)) {
		StreamLog("Wrong source key.");
		return false;
	}

	if (request.Source == request.Destination) {
		StreamLog("Loop call.");
		return false;
	}

	_userSession = session;
	_state = State::ReadyToSendGateInit;
	_destination = request.Destination;
	_initRequest = initRequest;

	_hasGateSession = true;
	_user->StartStreamGateSession();

	StreamLog("Success.");

	return true;
}

int32_t StreamHandler::ProcessGateStreamInit(
	const CowBuffer<uint8_t> initRequest)
{
	StreamLog("ProcessGateStreamInit.");

	StreamHandshake::InitRequest request;
	bool parseResult = StreamHandshake::ParseInitRequest(
		initRequest,
		request);

	if (!parseResult) {
		return STREAM_INIT_RESPONSE_PARSING_FAILURE;
	}

	int32_t checkCode = _user->CheckInboundCall(request);

	if (checkCode != STREAM_INIT_RESPONSE_WAITING_FOR_ANSWER) {
		return checkCode;
	}

	if (_state != State::Closed || _hasGateSession) {
		return STREAM_INIT_RESPONSE_USER_BUSY;
	}

	if (!_user->BroadcastStreamRequest(initRequest)) {
		return STREAM_INIT_RESPONSE_USER_OFFLINE;
	}

	_hasGateSession = true;
	_state = State::WaitingForUserAnswer;
	_initRequest = initRequest;
	return STREAM_INIT_RESPONSE_WAITING_FOR_ANSWER;
}

void StreamHandler::NotifyUserSessionClosed(ServerSession *session)
{
	if (_userSession != session) {
		return;
	}

	StreamLog("User session closed.");

	_userSession = nullptr;
	EndStream(nullptr);
}

bool StreamHandler::ProcessUserStreamResponse(
	const CowBuffer<uint8_t> initResponse,
	ServerSession *session)
{
	StreamLog("ProcessUserStreamResponse.");

	if (_state != State::WaitingForUserAnswer) {
		return false;
	}

	_state = State::OpenedSession;
	_userSession = session;

	PushQueue(initResponse);

	_user->BroadcastStreamEnd(_userSession);
	return true;
}

void StreamHandler::CheckNewSession(ServerSession *session)
{
	if (_state != State::WaitingForUserAnswer) {
		return;
	}

	session->SendStreamRequest(_initRequest);
}

bool StreamHandler::ProcessGateKeepAlive(const CowBuffer<uint8_t> buffer)
{
	StreamLog("ProcessGateKeepAlive.");

	bool allowedState =
		_state == State::WaitingForGatePeerResponse ||
		_state == State::WaitingForUserAnswer ||
		_state == State::OpenedSession;

	if (!allowedState) {
		return false;
	}

	if (buffer.Size() != sizeof(int32_t)) {
		return false;
	}

	int32_t command = SetProtoEndian(*buffer.SwitchType<int32_t>());

	if (command != GATE_STREAM_KEEP_ALIVE) {
		return false;
	}

	if (_sentGateKeepAlive) {
		_sentGateKeepAlive = false;
	} else {
		SendGateKeepAlive();
	}

	return true;
}

void StreamHandler::SendGateKeepAlive()
{
	CowBuffer<uint8_t> buffer(sizeof(int32_t));
	*buffer.SwitchType<int32_t>() = SetProtoEndian<int32_t>(
		GATE_STREAM_KEEP_ALIVE);

	PushQueue(buffer);
}

bool StreamHandler::ProcessGateStreamEnd(const CowBuffer<uint8_t> buffer)
{
	StreamLog("ProcessGateStreamEnd.");

	if (_state == State::Closed) {
		return false;
	}

	if (buffer.Size() != sizeof(int32_t)) {
		return false;
	}

	int32_t command = SetProtoEndian(*buffer.SwitchType<int32_t>());

	if (command != GATE_STREAM_END) {
		return false;
	}

	// TODO: processing.

	return true;
}

void StreamHandler::SendGateStreamEnd()
{
	CowBuffer<uint8_t> buffer(sizeof(int32_t));
	*buffer.SwitchType<int32_t>() = SetProtoEndian<int32_t>(
		GATE_STREAM_END);

	PushQueue(buffer);
}

bool StreamHandler::ProcessOpenedSession(const CowBuffer<uint8_t> buffer)
{
	if (buffer.Size() < sizeof(int32_t)) {
		return false;
	}

	int32_t command = SetProtoEndian(*buffer.SwitchType<int32_t>());

	switch (command) {
	case GATE_STREAM_KEEP_ALIVE:
		return ProcessGateKeepAlive(buffer);
	default:
		break;
	}

	return true;
}

void StreamHandler::PushQueue(const CowBuffer<uint8_t> buffer)
{
	QueueItem *item = new QueueItem;
	item->Next = nullptr;
	item->Data = buffer;

	if (!_queueFirst) {
		_queueFirst = item;
		_queueLast = item;
	} else {
		_queueLast->Next = item;
		_queueLast = item;
	}
}

void StreamHandler::StreamLog(String message)
{
	Log(LogLevel::Debug, _userName + " StreamHandler", message);
}
