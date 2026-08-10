#include "InboundGateSession.hpp"

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "../Protocol/GateParser.hpp"
#include "../Common/Exception.hpp"
#include "../Common/File.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/Log.hpp"
#include "../Common/Endianness.hpp"
#include "../Common/Hex.hpp"

InboundTaskBase *InboundTaskBase::GetTask(
	int32_t command,
	UserDB *users,
	GateSecurityModule *secMod)
{
	switch (command) {
	case GATE_COMMAND_MESSAGE:
		return new InboundTaskReceiveMessage(users, secMod);
	default:
		break;
	}

	return nullptr;
}

InboundTaskReceiveMessage::InboundTaskReceiveMessage(
	UserDB *users,
	GateSecurityModule *secMod)
{
	_users = users;
	_securityModule = secMod;

	_state = State::WaitForHeader;
	_silentBlock = false;
}

bool InboundTaskReceiveMessage::HasData()
{
	return _response.Size() > 0;
}

CowBuffer<uint8_t> InboundTaskReceiveMessage::GetData()
{
	CowBuffer<uint8_t> buffer = _response;
	_response = CowBuffer<uint8_t>();
	return buffer;
}

bool InboundTaskReceiveMessage::ProcessData(const CowBuffer<uint8_t> buffer)
{
	if (_state == State::WaitForHeader) {
		return ProcessWaitHeader(buffer);
	} else if (_state == State::WaitForBody) {
		return ProcessWaitBody(buffer);
	}

	return false;
}

bool InboundTaskReceiveMessage::ProcessWaitHeader(
	const CowBuffer<uint8_t> buffer)
{
	GateCommandMessage::Header headerData;
	bool parseResult =
		GateCommandMessage::ParseHeader(buffer, headerData);

	if (!parseResult) {
		return false;
	}

	parseResult = Message::X25519::ParseHeader(
		headerData.MessageHeader,
		_header);

	if (!parseResult) {
		SendVerificationCode(
			GATE_MESSAGE_HEADER_REJECT_INVALID_HEADER);
		return true;
	}

	if (headerData.MessageHeader.Size() != _header.HeaderSize) {
		SendVerificationCode(
			GATE_MESSAGE_HEADER_REJECT_INVALID_HEADER);
		return true;
	}

	String hostName;

	bool res = Message::SplitFullUserName(
		_header.Destination,
		_userName,
		hostName);

	if (!res) {
		SendVerificationCode(
			GATE_MESSAGE_HEADER_REJECT_INVALID_DESTINATION_USER);
		return true;
	}

	String sourceName;
	String sourceHost;

	res = Message::SplitFullUserName(
		_header.Source,
		sourceName,
		sourceHost);

	if (!res) {
		SendVerificationCode(
			GATE_MESSAGE_HEADER_REJECT_INVALID_HEADER);
		return true;
	}

	if (sourceHost != _securityModule->GetFullHostName()) {
		SendVerificationCode(GATE_MESSAGE_HEADER_REJECT);
		return true;
	}

	User *user = _users->GetUser(_userName);

	if (!user) {
		SendVerificationCode(
			GATE_MESSAGE_HEADER_REJECT_INVALID_DESTINATION_USER);
		return true;
	}

	int32_t resultCode = user->CheckInboundMessage(
		_header,
		Crypto::GetHash(
			headerData.MessageHeader,
			(int)ObjectStorage::Constants::IDSize).Pointer());

	if (resultCode == GATE_MESSAGE_HEADER_ACCEPT) {
		_receivedPart = headerData.MessageHeader;
		_state = State::WaitForBody;
	}

	SendVerificationCode(resultCode);
	return true;
}

bool InboundTaskReceiveMessage::ProcessWaitBody(
	const CowBuffer<uint8_t> buffer)
{
	if (buffer.Size() != _header.MessageSize) {
		SendVerificationCode(GATE_MESSAGE_BODY_REJECT_INVALID_SIZE);
	} else {
		_receivedPart = _receivedPart.Concat(buffer);

		if (!_silentBlock) {
			User *user = _users->GetUser(_userName);

			if (!user) {
				SendVerificationCode(
					GATE_MESSAGE_BODY_REJECT);
			} else {
				user->DeliverMessage(_header, _receivedPart);
				SendVerificationCode(GATE_MESSAGE_BODY_ACCEPT);
			}
		} else {
			SendVerificationCode(GATE_MESSAGE_BODY_ACCEPT);
		}
	}

	_state = State::WaitForHeader;
	_silentBlock = false;
	return true;
}

void InboundTaskReceiveMessage::SendVerificationCode(int32_t code)
{
	if (code == GATE_MESSAGE_HEADER_REJECT_SILENTBLOCK) {
		_state = State::WaitForBody;
		_silentBlock = true;
		code = GATE_MESSAGE_HEADER_ACCEPT;
	}

	GateCommandMessage::VerificationCode codeStruct;
	codeStruct.Code = code;

	_response = GateCommandMessage::BuildCode(codeStruct);
}

InboundGateSession::InboundGateSession(
	int fd,
	uint32_t ipv4,
	InboundGateSessionStorage *storage,
	EventDispatcher *dispatcher,
	UserDB *users,
	Config *config,
	RateLimiter *rateLimiter) :
	_securityModule(dispatcher)
{
	SetInterval(60000);
	SetTimestamp(GetMonotonicMillisecondTime());

	_fd = fd;
	_ipv4 = ipv4;
	_storage = storage;
	_dispatcher = dispatcher;
	_users = users;
	_config = config;
	_rateLimiter = rateLimiter;

	_reader = nullptr;
	_writer = nullptr;

	_protocol = nullptr;
	_expectedChunkSize = true;

	_task = nullptr;

	_dispatcher->RegisterTimeProcessor(this);
	_dispatcher->RegisterDescriptorProcessor(this);

	_securityModule.SetUser(this);
	_securityModule.SetKnownPeerIP(_ipv4);

	InboundGateLog("Session opened.");

	SendInit();
}

InboundGateSession::~InboundGateSession()
{
	InboundGateLog("Session closed.");

	_securityModule.SetUser(nullptr);

	if (_task) {
		delete _task;
		_task = nullptr;
	}

	if (_protocol) {
		delete _protocol;
		_protocol = nullptr;
	}

	_dispatcher->UnregisterDescriptorProcessor(this);
	_dispatcher->UnregisterTimeProcessor(this);

	if (_reader) {
		delete _reader;
		_reader = nullptr;
	}

	if (_writer) {
		delete _writer;
		_writer = nullptr;
	}

	shutdown(_fd, SHUT_RDWR);
	close(_fd);
	_fd = -1;
}

int InboundGateSession::GetDescriptor()
{
	return _fd;
}

bool InboundGateSession::RequestRead()
{
	return _reader;
}

bool InboundGateSession::RequestWrite()
{
	if (_writer) {
		return true;
	}

	if (_protocol && _protocol->HasOutput()) {
		return true;
	}

	if (_protocol && _task && _task->HasData()) {
		return true;
	}

	return false;
}

void InboundGateSession::ProcessRead()
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

	bool readingEnded = _reader->ReadingEnd();

	if (!readingEnded) {
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
	} else if (_state == State::HandshakeWaitSynSize) {
		success = ProcessHandshakeSynSize(buffer);
	} else if (_state == State::HandshakeWaitSyn) {
		success = ProcessHandshakeSyn(buffer);
	} else if (_state == State::HandshakeWaitVerificationResponse) {
		success = ProcessHandshakeVerificationResponse(buffer);
	} else {
		THROW("Invalid inbound gate session state.");
	}

	if (!success) {
		_storage->MarkSessionForRemoval(this);
	}
}

void InboundGateSession::ProcessWrite()
{
	SetTimestamp(GetMonotonicMillisecondTime());

	if (_protocol && _task && _task->HasData()) {
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

		CowBuffer<uint8_t> sizeBuffer(sizeof(int32_t));
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

	bool writingEnded = _writer->WritingEnd();

	if (!writingEnded) {
		return;
	}

	delete _writer;
	_writer = nullptr;

	if (_state == State::WriteAllAndExit) {
		_storage->MarkSessionForRemoval(this);
	}
}

void InboundGateSession::ProcessTimeEvent()
{
	if (_state == State::HandshakeWaitPeerResolving) {
		return;
	}

	_rateLimiter->RecordSessionTimeout(_ipv4);
	InboundGateLog("Timeout.");
	_storage->MarkSessionForRemoval(this);
}

void InboundGateSession::ResolveCompleted()
{
	if (_state != State::HandshakeWaitPeerResolving) {
		THROW("Resolve handler was called in wrong state.");
	}

	VerifyPeer();
}

void InboundGateSession::SendInit()
{
	bool allowed = _rateLimiter->IsAllowed(_ipv4);

	GateHandshakeStatus::Data data;

	if (allowed) {
		_rateLimiter->RecordRequest(_ipv4);
		data.Status = GATE_HANDSHAKE_INIT_PROCEED;
		_state = State::HandshakeWaitSynSize;
		_reader = new StreamReader(_fd, sizeof(uint32_t) + 1);
	} else {
		data.Status = GATE_HANDSHAKE_INIT_REQUEST_RATE_LIMIT_REACHED;
		_state = State::WriteAllAndExit;
	}

	CowBuffer<uint8_t> buffer = GateHandshakeStatus::BuildData(data);

	Crypto::GenerateRandomData(1, &_outScramblerInit, false);

	CowBuffer<uint8_t> scramblerBuffer(1);
	*scramblerBuffer.Pointer() = _outScramblerInit;

	_outScramblerInit = Crypto::ApplyScrambler(
		buffer.Pointer(),
		buffer.Size(),
		_outScramblerInit);

	_writer = new StreamWriter(_fd, scramblerBuffer.Concat(buffer));
}

bool InboundGateSession::ProcessHandshakeSynSize(CowBuffer<uint8_t> buffer)
{
	if (buffer.Size() != sizeof(uint32_t) + 1) {
		return false;
	}

	_inScramblerInit = buffer[0];

	buffer = buffer.Slice(1, buffer.Size() - 1);

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

bool InboundGateSession::ProcessHandshakeSyn(CowBuffer<uint8_t> buffer)
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
		return false;
	}

	if (syn.ProtocolVersion != 0) {
		SendUnsupportedProtocolVersion();
		return true;
	}

	if (syn.EncryptionScheme != Crypto::X25519::SCHEME_ID) {
		SendUnsupportedEncryptionScheme();
		return true;
	}

	_securityModule.SetPeerReportedFullHostName(syn.ServerName);

	if (_securityModule.Failure()) {
		SendVerificationFailure();
		return true;
	}

	_peerPublicKey = syn.Key;
	_salt1 = syn.Salt;

	_peerSynBuffer = buffer;
	_peerSynSignature = syn.Signature;

	VerifyPeer();
	return true;
}

void InboundGateSession::SendUnsupportedProtocolVersion()
{
	GateHandshakeSyn::Data data;
	data.Stat = GateHandshakeSyn::Data::Status::ErrorCode;
	data.ErrorCode = GATE_HANDSHAKE_UNSUPPORTED_PROTOCOL_VERSION;

	_state = State::WriteAllAndExit;
	CreateSizePrefixAndSend(GateHandshakeSyn::BuildData(data));
}

void InboundGateSession::SendUnsupportedEncryptionScheme()
{
	GateHandshakeSyn::Data data;
	data.Stat = GateHandshakeSyn::Data::Status::ErrorCode;
	data.ErrorCode = GATE_HANDSHAKE_UNSUPPORTED_ENCRYPTION_SCHEME;

	_state = State::WriteAllAndExit;
	CreateSizePrefixAndSend(GateHandshakeSyn::BuildData(data));
}

void InboundGateSession::SendVerificationFailure()
{
	GateHandshakeSyn::Data data;
	data.Stat = GateHandshakeSyn::Data::Status::ErrorCode;
	data.ErrorCode = GATE_HANDSHAKE_VERIFICATION_FAILURE;

	_state = State::WriteAllAndExit;
	CreateSizePrefixAndSend(GateHandshakeSyn::BuildData(data));
}

void InboundGateSession::SendSyn()
{
	Crypto::X25519::GenerateEphemeralKeyPair(
		_ephemeralPrivateKey,
		_ephemeralPublicKey);

	GateHandshakeSyn::Data syn;

	syn.Stat = GateHandshakeSyn::Data::Status::Syn;
	syn.ProtocolVersion = 0;
	syn.EncryptionScheme = Crypto::X25519::SCHEME_ID;
	syn.ServerName = _config->GetHostName();
	syn.Key = _ephemeralPublicKey;

	_salt2 = CowBuffer<uint8_t>(32);

	Crypto::GenerateRandomData(
		_salt2.Size(),
		_salt2.Pointer(),
		false);

	syn.Salt = _salt2;
	// TODO: signature processing.

	CreateSizePrefixAndSend(GateHandshakeSyn::BuildData(syn));

	_state = State::HandshakeWaitVerificationResponse;
	_reader = new StreamReader(_fd, sizeof(int32_t));
}

void InboundGateSession::VerifyPeer()
{
	if (_securityModule.Failure()) {
		SendVerificationFailure();
		return;
	}

	if (_securityModule.NeedSRV()) {
		_state = State::HandshakeWaitPeerResolving;
		_securityModule.RunSRV();
		return;
	}

	if (_securityModule.NeedA()) {
		_state = State::HandshakeWaitPeerResolving;
		_securityModule.RunA();
		return;
	}

	if (_securityModule.NeedParams()) {
		_state = State::HandshakeWaitPeerResolving;
		_securityModule.RunParams();
		return;
	}

	_securityModule.ValidateSyn(_peerSynBuffer, _peerSynSignature);

	if (_securityModule.Failure()) {
		SendVerificationFailure();
		return;
	}

	_securityModule.RunFullValidation();

	if (_securityModule.Failure()) {
		SendVerificationFailure();
		return;
	}

	SendSyn();
}

bool InboundGateSession::ProcessHandshakeVerificationResponse(
	CowBuffer<uint8_t> buffer)
{
	_inScramblerInit = Crypto::ApplyScrambler(
		buffer.Pointer(),
		buffer.Size(),
		_inScramblerInit);

	GateHandshakeStatus::Data data;
	bool parseResult = GateHandshakeStatus::ParseData(buffer, data);

	if (!parseResult) {
		return false;
	}

	if (data.Status == GATE_HANDSHAKE_VERIFICATION_FAILURE) {
		InboundGateLog("Peer rejected syn.");
		return false;
	} else if (data.Status != GATE_HANDSHAKE_VERIFICATION_SUCCESS) {
		return false;
	}

	bool validKeys = Crypto::X25519::GenerateSessionKeys(
		_ephemeralPrivateKey,
		_ephemeralPublicKey,
		_peerPublicKey,
		_salt1.Concat(_salt2),
		_inES.Key,
		_outES.Key,
		true);

	if (!validKeys) {
		InboundGateLog("Key exchange failed.");
		return false;
	}

	Crypto::X25519::InitNonce(_outES.Nonce);
	memset(_inES.Nonce, 0, Crypto::X25519::NONCE_SIZE);

	_state = State::OpenedSession;
	_reader = new StreamReader(_fd, sizeof(uint32_t));
	_protocol = new GateProtocol(&_outES, &_inES);

	_protocol->SetInputSizeLimit(_config->GetMessageSizeLimit());
	return true;
}

bool InboundGateSession::ProcessSessionInput(CowBuffer<uint8_t> buffer)
{
	if (!buffer.Size()) {
		return false;
	}

	_inScramblerInit = Crypto::ApplyScrambler(
		buffer.Pointer(),
		buffer.Size(),
		_inScramblerInit);

	if (_expectedChunkSize) {
		if (buffer.Size() != sizeof(uint32_t)) {
			return false;
		}

		uint32_t size = SetProtoEndian(*buffer.SwitchType<uint32_t>());

		if (!size || size > 4096) {
			return false;
		}

		_reader = new StreamReader(_fd, size);
		_expectedChunkSize = false;
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
	_expectedChunkSize = true;

	if (!_protocol->HasInputBuffer()) {
		return true;
	}

	CowBuffer<uint8_t> inputBuffer = _protocol->GetInputBuffer();

	if (!_task) {
		if (inputBuffer.Size() < sizeof(int32_t)) {
			return false;
		}

		int32_t command =
			SetProtoEndian(*inputBuffer.SwitchType<int32_t>());

		_task = InboundTaskBase::GetTask(
			command,
			_users,
			&_securityModule);

		if (!_task) {
			return false;
		}
	}

	bool processingResult = _task->ProcessData(inputBuffer);

	if (!processingResult) {
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

void InboundGateSession::CreateSizePrefixAndSend(CowBuffer<uint8_t> buffer)
{
	CowBuffer<uint8_t> sizeBuffer(sizeof(uint32_t));
	*sizeBuffer.SwitchType<uint32_t>() =
		SetProtoEndian<uint32_t>(buffer.Size());

	buffer = sizeBuffer.Concat(buffer);

	_outScramblerInit = Crypto::ApplyScrambler(
		buffer.Pointer(),
		buffer.Size(),
		_outScramblerInit);

	_writer = new StreamWriter(_fd, buffer);
}

void InboundGateSession::InboundGateLog(String message)
{
	Log(LogLevel::Info,
		"Inbound gate from " + IPToString(_ipv4),
		message);
}
