#include "OutboundGateSession.hpp"

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#include "../Message/Message.hpp"
#include "../Protocol/GateParser.hpp"
#include "../Protocol/StreamParser.hpp"
#include "../Common/Exception.hpp"
#include "../Common/File.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/Hex.hpp"
#include "../Common/Log.hpp"
#include "../Common/Endianness.hpp"

TaskProcessChannel::TaskProcessChannel()
{
	ReportTarget = nullptr;
	_state = State::Init;
	_hasOutput = false;
}

String TaskProcessChannel::GetConnectionDestination()
{
	return Destination;
}

void TaskProcessChannel::ReportConnectionFailure()
{
	if (!ReportTarget) {
		THROW("Report target must not be NULL.");
	}

	if (MustReportFailure()) {
		ReportTarget->ReportConnectionFailure();
		MarkFailureReport();
	}
}

void TaskProcessChannel::ReportRequestRateLimit()
{
	if (!ReportTarget) {
		THROW("Report target must not be NULL.");
	}

	if (MustReportFailure()) {
		ReportTarget->ReportRequestRateLimit();
		MarkFailureReport();
	}
}

bool TaskProcessChannel::HasData()
{
	if (!ReportTarget) {
		THROW("Report target must not be NULL.");
	}

	if (_state == State::Init) {
		return true;
	}

	return _hasOutput;
}

CowBuffer<uint8_t> TaskProcessChannel::GetData()
{
	if (!ReportTarget) {
		THROW("Report target must not be NULL.");
	}

	if (_state == State::Init) {
		for (;;) {
			_currentMessage = ReportTarget->GetMessageForChannel();

			bool parseResult = Message::X25519::ParseHeader(
				_currentMessage,
				_header);

			if (parseResult) {
				break;
			}

			bool cont = ReportTarget->ReportDeliveryStatus(
				false,
				GATE_MESSAGE_HEADER_REJECT_INVALID_HEADER);

			if (!cont) {
				MarkFailureReport();
				return CowBuffer<uint8_t>();
			}
		}

		GateCommandMessage::Header header;
		header.MessageHeader =
			_currentMessage.Slice(0, _header.HeaderSize);

		_state = State::SentMessageHeader;
		_hasOutput = false;
		return GateCommandMessage::BuildHeader(header);
	}

	if (_state == State::SentMessageHeader) {
		_state = State::SentMessageBody;
		_hasOutput = false;
		return _currentMessage.Slice(
			_header.HeaderSize,
			_currentMessage.Size() - _header.HeaderSize);
	}

	return CowBuffer<uint8_t>();
}

bool TaskProcessChannel::ProcessData(const CowBuffer<uint8_t> buffer)
{
	if (!ReportTarget) {
		THROW("Report target must not be NULL.");
	}

	if (_state == State::SentMessageHeader) {
		GateCommandMessage::Text text;
		bool parseResult = GateCommandMessage::ParseText(buffer, text);

		if (parseResult) {
			Log(LogLevel::Info,
				"Outbound message rejected",
				text.Text);
			return true;
		}

		GateCommandMessage::VerificationCode code;
		parseResult = GateCommandMessage::ParseCode(buffer, code);

		if (!parseResult) {
			return false;
		}

		if (code.Code > GATE_MESSAGE_HEADER_REJECT_EXISTS) {
			return false;
		}

		if (code.Code == GATE_MESSAGE_HEADER_ACCEPT) {
			_hasOutput = true;
			return true;
		}

		bool continueProc = ReportTarget->ReportDeliveryStatus(
			false,
			code.Code);
		_hasOutput = false;
		_state = State::Init;

		if (!continueProc) {
			MarkFailureReport();
		}

		return continueProc;
	}

	if (_state == State::SentMessageBody) {
		GateCommandMessage::VerificationCode code;
		bool parseResult = GateCommandMessage::ParseCode(buffer, code);

		if (!parseResult) {
			return false;
		}

		if (code.Code > GATE_MESSAGE_BODY_REJECT_INVALID_SIZE) {
			return false;
		}

		bool continueProc;

		if (code.Code == GATE_MESSAGE_BODY_ACCEPT) {
			continueProc = ReportTarget->ReportDeliveryStatus(
				true,
				0);
		} else {
			continueProc = ReportTarget->ReportDeliveryStatus(
				false,
				GATE_MESSAGE_HEADER_REJECT);
		}

		_hasOutput = false;
		_state = State::Init;

		if (!continueProc) {
			MarkFailureReport();
		}

		return continueProc;
	}

	return false;
}

OutboundGateSession::OutboundGateSession(
	EventDispatcher *dispatcher,
	Config *config,
	OutboundGateSessionStorage *storage,
	OutboundTaskBase *task) :
	_securityModule(dispatcher)
{
	SetInterval(task->GetTimeoutInterval());
	SetTimestamp(GetMonotonicMillisecondTime());

	_fd = -1;
	_dispatcher = dispatcher;
	_config = config;
	_storage = storage;
	_task = task;

	if (!_task) {
		THROW("Task can not be NULL.");
	}

	_state = State::WaitingForDestinationResolve;
	_requestedAResolve = false;

	_reader = nullptr;
	_writer = nullptr;

	_protocol = nullptr;
	_expectChunkSize = true;

	_securityModule.SetUser(this);

	_dispatcher->RegisterTimeProcessor(this);

	OutboundGateLog("Session opened.");
}

OutboundGateSession::~OutboundGateSession()
{
	OutboundGateLog("Session closed.");

	_dispatcher->UnregisterTimeProcessor(this);

	_securityModule.SetUser(nullptr);

	if (_fd != -1) {
		_dispatcher->UnregisterDescriptorProcessor(this);
		shutdown(_fd, SHUT_RDWR);
		close(_fd);
		_fd = -1;
	}

	if (_task) {
		try {
			_task->ReportConnectionFailure();
		} catch (Exception &ex) {
			OutboundGateLog("Task error: " + ex.Message());
		}

		_task->NotifyGateSessionClosed();

		if (_task->MustBeDeleted()) {
			delete _task;
		}

		_task = nullptr;
	}

	if (_protocol) {
		delete _protocol;
		_protocol = nullptr;
	}

	if (_reader) {
		delete _reader;
		_reader = nullptr;
	}

	if (_writer) {
		delete _writer;
		_writer = nullptr;
	}
}

void OutboundGateSession::CompleteInitialization()
{
	StartConnection();
}

int OutboundGateSession::GetDescriptor()
{
	return _fd;
}

bool OutboundGateSession::RequestRead()
{
	return _reader;
}

bool OutboundGateSession::RequestWrite()
{
	if (_writer) {
		return true;
	}

	if (_state == State::WaitingForConnect) {
		return true;
	}

	if (_protocol && _protocol->HasOutput()) {
		return true;
	}

	if (_protocol && _task->HasData()) {
		return true;
	}

	return false;
}

void OutboundGateSession::ProcessRead()
{
	SetTimestamp(GetMonotonicMillisecondTime());

	if (!_reader) {
		THROW("Reader is NULL.");
	}

	bool success = _reader->Read();

	if (!success) {
		delete _reader;
		_reader = nullptr;
		_storage->MarkSessionForRemoval(this);
		return;
	}

	bool readComplete = _reader->ReadingEnd();

	if (!readComplete) {
		return;
	}

	CowBuffer<uint8_t> buffer = _reader->GetBuffer();

	delete _reader;
	_reader = nullptr;

	if (_state != State::OpenedSession && _writer) {
		_storage->MarkSessionForRemoval(this);
		return;
	}

	if (_state == State::OpenedSession) {
		success = ProcessSessionInput(buffer);
	} else if (_state == State::HandshakeWaitInit) {
		success = ProcessInit(buffer);
	} else if (_state == State::HandshakeWaitSynSize) {
		success = ProcessSynSize(buffer);
	} else if (_state == State::HandshakeWaitSyn) {
		success = ProcessSyn(buffer);
	} else {
		THROW("Invalid gate session state.");
	}

	if (!success) {
		_storage->MarkSessionForRemoval(this);
	}
}

void OutboundGateSession::ProcessWrite()
{
	SetTimestamp(GetMonotonicMillisecondTime());

	if (_state == State::WaitingForConnect) {
		ProcessConnect();
		return;
	}

	if (_protocol && _task->HasData()) {
		CowBuffer<uint8_t> buf = _task->GetData();

		if (buf.Size()) {
			_protocol->AddBufferForOutput(buf);
		} else {
			_storage->MarkSessionForRemoval(this);
			return;
		}
	}

	if (!_writer) {
		if (!(_protocol && _protocol->HasOutput())) {
			THROW("Writer is NULL.");
		}

		CowBuffer<uint8_t> outBuffer = _protocol->GetOutputBuffer();

		CowBuffer<uint8_t> sizeBuffer(sizeof(uint32_t));
		*sizeBuffer.SwitchType<uint32_t>() =
			SetProtoEndian<uint32_t>(outBuffer.Size());

		outBuffer = sizeBuffer.Concat(outBuffer);

		_outScramblerInit = Crypto::ApplyScrambler(
			outBuffer.Pointer(),
			outBuffer.Size(),
			_outScramblerInit);

		_writer = new StreamWriter(_fd, outBuffer);
	}

	bool success = _writer->Write();

	if (!success) {
		delete _writer;
		_writer = nullptr;
		_storage->MarkSessionForRemoval(this);
		return;
	}

	bool writeComplete = _writer->WritingEnd();

	if (!writeComplete) {
		return;
	}

	delete _writer;
	_writer = nullptr;

	if (_state == State::SendAllAndShutdown) {
		_storage->MarkSessionForRemoval(this);
	}
}

void OutboundGateSession::ProcessTimeEvent()
{
	if (_state == State::WaitingForDestinationResolve) {
		// Resolver deletion on resolve will lead to hangup.
		return;
	}

	if (_task->TimeoutAction()) {
		return;
	}

	OutboundGateLog("Timeout.");
	_storage->MarkSessionForRemoval(this);
}

void OutboundGateSession::ResolveCompleted()
{
	if (_state != State::WaitingForDestinationResolve) {
		THROW("Invalid state in ResolveCompleted.");
	}

	// If A record type request ended with failure AAAA must be tried.
	// Resolve call with _requestedAResolve == true will try AAAA
	// request. Clearing possible error from A request.
	if (_requestedAResolve) {
		_securityModule.ClearFailure();
	}

	Resolve();
}

void OutboundGateSession::StartConnection()
{
	String fullName = _task->GetConnectionDestination();

	String userName;
	String hostName;

	bool parseResult = Message::SplitFullUserName(
		fullName,
		userName,
		hostName);

	if (!parseResult) {
		THROW("Invalid name in server database: " + fullName + ".");
	}

	_securityModule.SetKnownPeerFullHostName(hostName);

	Resolve();
}

void OutboundGateSession::Resolve()
{
	if (_securityModule.Failure()) {
		_task->ReportConnectionFailure();
		_storage->MarkSessionForRemoval(this);
		return;
	}

	if (_securityModule.NeedSRV()) {
		_state = State::WaitingForDestinationResolve;
		_securityModule.RunSRV();
		return;
	}

	if (_securityModule.NeedA() && !_requestedAResolve) {
		_requestedAResolve = true;
		_state = State::WaitingForDestinationResolve;
		_securityModule.RunA();
		return;
	}

	_requestedAResolve = false;

	if (_securityModule.NeedAAAA()) {
		_state = State::WaitingForDestinationResolve;
		_securityModule.RunAAAA();
		return;
	}

	if (_securityModule.NeedParams()) {
		_state = State::WaitingForDestinationResolve;
		_securityModule.RunParams();
		return;
	}

	// Any other address is unknown, connecting to known address.
	if (_securityModule.IsIPOnlyHost()) {
		_securityModule.AcceptHostNameAsIP();

		if (_securityModule.Failure()) {
			_task->ReportConnectionFailure();
			_storage->MarkSessionForRemoval(this);
			return;
		}
	}

	_securityModule.SetKnownPeerIP(_securityModule.GetDNSReportedIP());

	if (_securityModule.Failure()) {
		_task->ReportConnectionFailure();
		_storage->MarkSessionForRemoval(this);
		return;
	}

	TryConnect();
}

void OutboundGateSession::TryConnect()
{
	String portString = _securityModule.GetSRVReportedServiceName();

	if (!Message::VerifyPortName(portString)) {
		_task->ReportConnectionFailure();
		_storage->MarkSessionForRemoval(this);
		return;
	}

	int portNumber = atoi(portString.CStr());

	int addrLen;
	struct sockaddr_storage *addr =
		_securityModule.GetDNSReportedIP().GetStructSockaddr(
			htons(portNumber),
			addrLen);

	_fd = socket(addr->ss_family, SOCK_STREAM, 0);

	if (_fd == -1) {
		delete addr;
		OutboundGateLog("Failed to create socket.");
		_task->ReportConnectionFailure();
		_storage->MarkSessionForRemoval(this);
		return;
	}

	MakeNonblocking(_fd);

	int res = connect(_fd, (struct sockaddr*)addr, addrLen);

	delete addr;

	if (res == -1) {
		if (errno == EINPROGRESS) {
			OutboundGateLog("Connection in progress.");
			_state = State::WaitingForConnect;
			_dispatcher->RegisterDescriptorProcessor(this);
			return;
		}

		close(_fd);
		_fd = -1;

		_task->ReportConnectionFailure();
		_storage->MarkSessionForRemoval(this);
		OutboundGateLog(String("Connect: ") + strerror(errno) + ".");
		return;
	}

	_dispatcher->RegisterDescriptorProcessor(this);
	SetupHandshakeWaitInit();
}

void OutboundGateSession::ProcessConnect()
{
	int connRes;
	socklen_t connResSize = sizeof(connRes);

	int res = getsockopt(_fd, SOL_SOCKET, SO_ERROR, &connRes, &connResSize);

	bool success = true;

	if (res == -1) {
		OutboundGateLog("Getsockopt failure.");
		success = false;
	}

	if (success) {
		if (connRes) {
			success = false;
			OutboundGateLog(
				String("Connect: ") + strerror(connRes) + ".");
		}
	}

	if (success) {
		OutboundGateLog("Connection established.");
		SetupHandshakeWaitInit();
		return;
	}

	_dispatcher->UnregisterDescriptorProcessor(this);
	close(_fd);
	_fd = -1;

	_task->ReportConnectionFailure();
	_storage->MarkSessionForRemoval(this);
}

void OutboundGateSession::SetupHandshakeWaitInit()
{
	_state = State::HandshakeWaitInit;
	_reader = new StreamReader(_fd, sizeof(int32_t) + 1);
}

bool OutboundGateSession::ProcessInit(const CowBuffer<uint8_t> buffer)
{
	if (buffer.Size() < 2) {
		return false;
	}

	_inScramblerInit = buffer[0];

	CowBuffer<uint8_t> statusBuffer = buffer.Slice(1, buffer.Size() - 1);

	_inScramblerInit = Crypto::ApplyScrambler(
		statusBuffer.Pointer(),
		statusBuffer.Size(),
		_inScramblerInit);

	GateHandshakeStatus::Data status;
	bool parseResult = GateHandshakeStatus::ParseData(statusBuffer, status);

	if (!parseResult) {
		return false;
	}

	if (status.Status == GATE_HANDSHAKE_INIT_REQUEST_RATE_LIMIT_REACHED) {
		_task->ReportRequestRateLimit();
		return false;
	} else if (status.Status == GATE_HANDSHAKE_INIT_PROCEED) {
		SendHandshakeSyn();
		return true;
	}

	return false;
}

void OutboundGateSession::SendHandshakeSyn()
{
	Crypto::X25519::GenerateEphemeralKeyPair(
		_ephemeralPrivateKey,
		_ephemeralPublicKey);

	_salt1 = CowBuffer<uint8_t>(32);

	Crypto::GenerateRandomData(_salt1.Size(), _salt1.Pointer(), false);

	GateHandshakeSyn::Data syn;

	syn.Stat = GateHandshakeSyn::Data::Status::Syn;
	syn.ProtocolVersion = 0;
	syn.EncryptionScheme = Crypto::X25519::SCHEME_ID;
	syn.ServerName = _config->GetHostName();
	syn.Key = _ephemeralPublicKey;
	syn.Salt = _salt1;
	// TODO: signature processing.

	CowBuffer<uint8_t> synBuffer = GateHandshakeSyn::BuildData(syn);

	CowBuffer<uint8_t> synSize(sizeof(uint32_t) + 1);
	*synSize.SwitchType<uint32_t>(1) =
		SetProtoEndian<uint32_t>(synBuffer.Size());

	Crypto::GenerateRandomData(1, synSize.Pointer(), false);

	synBuffer = synSize.Concat(synBuffer);

	_outScramblerInit = synBuffer[0];

	_outScramblerInit = Crypto::ApplyScrambler(
		synBuffer.Pointer(1),
		synBuffer.Size() - 1,
		_outScramblerInit);

	_writer = new StreamWriter(_fd, synBuffer);
	_reader = new StreamReader(_fd, sizeof(uint32_t));

	_state = State::HandshakeWaitSynSize;
}

bool OutboundGateSession::ProcessSynSize(CowBuffer<uint8_t> buffer)
{
	if (buffer.Size() != sizeof(uint32_t)) {
		return false;
	}

	_inScramblerInit = Crypto::ApplyScrambler(
		buffer.Pointer(),
		buffer.Size(),
		_inScramblerInit);

	uint32_t synSize = SetProtoEndian(*buffer.SwitchType<uint32_t>());

	if (!synSize || synSize > 2048) {
		return false;
	}

	_state = State::HandshakeWaitSyn;

	_reader = new StreamReader(_fd, synSize);
	return true;
}

bool OutboundGateSession::ProcessSyn(CowBuffer<uint8_t> buffer)
{
	_inScramblerInit = Crypto::ApplyScrambler(
		buffer.Pointer(),
		buffer.Size(),
		_inScramblerInit);

	GateHandshakeSyn::Data syn;
	bool parseResult = GateHandshakeSyn::ParseData(buffer, syn);

	if (!parseResult) {
		return false;
	}

	if (syn.Stat == GateHandshakeSyn::Data::Status::ErrorCode) {
		OutboundGateLog("Peer rejected syn.");
		return false;
	}

	if (syn.ProtocolVersion) {
		return false;
	}

	if (syn.EncryptionScheme != Crypto::X25519::SCHEME_ID) {
		return false;
	}

	_securityModule.SetPeerReportedFullHostName(syn.ServerName);

	if (_securityModule.Failure()) {
		SendVerificationStatus(false);
		_state = State::SendAllAndShutdown;
		return true;
	}

	_salt2 = syn.Salt;

	_securityModule.ValidateSyn(buffer, syn.Signature);

	if (_securityModule.Failure()) {
		SendVerificationStatus(false);
		_state = State::SendAllAndShutdown;
		return true;
	}

	_securityModule.RunFullValidation();

	if (_securityModule.Failure()) {
		SendVerificationStatus(false);
		_state = State::SendAllAndShutdown;
		return true;
	}

	bool keyExchangeSuccess = Crypto::X25519::GenerateSessionKeys(
		_ephemeralPrivateKey,
		_ephemeralPublicKey,
		syn.Key,
		_salt1.Concat(_salt2),
		_outES.Key,
		_inES.Key,
		false);

	if (!keyExchangeSuccess) {
		return false;
	}

	Crypto::X25519::InitNonce(_outES.Nonce);
	memset(_inES.Nonce, 0, Crypto::X25519::NONCE_SIZE);

	SendVerificationStatus(true);
	_state = State::OpenedSession;

	_protocol = new GateProtocol(&_outES, &_inES);
	_reader = new StreamReader(_fd, sizeof(uint32_t));

	_protocol->SetInputSizeLimit(_config->GetMessageSizeLimit());

	return true;
}

void OutboundGateSession::SendVerificationStatus(bool success)
{
	GateHandshakeStatus::Data data;

	if (success) {
		data.Status = GATE_HANDSHAKE_VERIFICATION_SUCCESS;
	} else {
		data.Status = GATE_HANDSHAKE_VERIFICATION_FAILURE;
	}

	CowBuffer<uint8_t> buffer = GateHandshakeStatus::BuildData(data);

	_outScramblerInit = Crypto::ApplyScrambler(
		buffer.Pointer(),
		buffer.Size(),
		_outScramblerInit);

	_writer = new StreamWriter(_fd, buffer);
}

bool OutboundGateSession::ProcessSessionInput(CowBuffer<uint8_t> buffer)
{
	if (!buffer.Size()) {
		return false;
	}

	_inScramblerInit = Crypto::ApplyScrambler(
		buffer.Pointer(),
		buffer.Size(),
		_inScramblerInit);

	if (_expectChunkSize) {
		if (buffer.Size() != sizeof(uint32_t)) {
			return false;
		}

		uint32_t size = SetProtoEndian(*buffer.SwitchType<uint32_t>());

		if (!size || size > 4096) {
			return false;
		}

		_reader = new StreamReader(_fd, size);
		_expectChunkSize = false;
		return true;
	}

	if (!_protocol) {
		THROW("Protocol is NULL.");
	}

	bool inputIsCorrect = _protocol->ProcessRead(buffer);

	if (!inputIsCorrect) {
		return false;
	}

	_reader = new StreamReader(_fd, sizeof(uint32_t));
	_expectChunkSize = true;

	if (!_protocol->HasInputBuffer()) {
		return true;
	}

	CowBuffer<uint8_t> inputBuffer = _protocol->GetInputBuffer();

	bool processingSuccess = _task->ProcessData(inputBuffer);

	if (!processingSuccess) {
		return false;
	}

	if (_task->HasData()) {
		CowBuffer<uint8_t> buf = _task->GetData();

		if (!buf.Size()) {
			return false;
		}

		_protocol->AddBufferForOutput(buf);
	}

	return true;
}

void OutboundGateSession::OutboundGateLog(String message)
{
	Log(LogLevel::Verbose,
		"Outbound gate to " + _task->GetConnectionDestination(),
		message);
}
