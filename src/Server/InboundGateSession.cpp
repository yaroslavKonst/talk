#include "InboundGateSession.hpp"

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "../Protocol/GateParser.hpp"
#include "../Common/Exception.hpp"
#include "../Common/File.hpp"
#include "../Common/Log.hpp"
#include "../Common/Endianness.hpp"

InboundGateSession::InboundGateSession(
	int fd,
	uint32_t ipv4,
	InboundGateSessionStorage *storage,
	EventDispatcher *dispatcher,
	Config *config,
	RateLimiter *rateLimiter)
{
	SetInterval(60000);
	SetTimestamp(GetMonotonicMillisecondTime());

	_fd = fd;
	_ipv4 = ipv4;
	_storage = storage;
	_dispatcher = dispatcher;
	_config = config;
	_rateLimiter = rateLimiter;

	_reader = nullptr;
	_writer = nullptr;

	_dispatcher->RegisterTimeProcessor(this);
	_dispatcher->RegisterDescriptorProcessor(this);

	InboundGateLog("Session opened.");

	SendInit();
}

InboundGateSession::~InboundGateSession()
{
	InboundGateLog("Session closed.");

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
	return _writer;
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

	if (!_writer) {
		THROW("Writer is NULL.");
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
	_rateLimiter->RecordSessionTimeout(_ipv4);
	InboundGateLog("Timeout.");
	_storage->MarkSessionForRemoval(this);
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

	GateHandshakeSyn::Data response;

	if (syn.ProtocolVersion != 0) {
		response = BuildUnsupportedProtocolVersion();
	} else if (syn.EncryptionScheme != Crypto::X25519::SCHEME_ID) {
		response = BuildUnsupportedEncryptionScheme();
	} else {
		_peerName = syn.ServerName;
		_peerPublicKey = syn.Key;
		_salt1 = syn.Salt;

		if (!VerifyPeer(buffer, syn.Signature)) {
			response = BuildVerificationFailure();
		} else {
			response = BuildSyn();
		}
	}

	CowBuffer<uint8_t> responseBuffer =
		GateHandshakeSyn::BuildData(response);

	CowBuffer<uint8_t> sizeBuffer(sizeof(uint32_t));
	*sizeBuffer.SwitchType<uint32_t>() =
		SetProtoEndian<uint32_t>(responseBuffer.Size());

	responseBuffer = sizeBuffer.Concat(responseBuffer);

	_outScramblerInit = Crypto::ApplyScrambler(
		responseBuffer.Pointer(),
		responseBuffer.Size(),
		_outScramblerInit);

	_writer = new StreamWriter(_fd, responseBuffer);
	return true;
}

GateHandshakeSyn::Data InboundGateSession::BuildUnsupportedProtocolVersion()
{
	GateHandshakeSyn::Data data;
	data.Stat = GateHandshakeSyn::Data::Status::ErrorCode;
	data.ErrorCode = GATE_HANDSHAKE_UNSUPPORTED_PROTOCOL_VERSION;

	_state = State::WriteAllAndExit;
	return data;
}

GateHandshakeSyn::Data InboundGateSession::BuildUnsupportedEncryptionScheme()
{
	GateHandshakeSyn::Data data;
	data.Stat = GateHandshakeSyn::Data::Status::ErrorCode;
	data.ErrorCode = GATE_HANDSHAKE_UNSUPPORTED_ENCRYPTION_SCHEME;

	_state = State::WriteAllAndExit;
	return data;
}

GateHandshakeSyn::Data InboundGateSession::BuildVerificationFailure()
{
	GateHandshakeSyn::Data data;
	data.Stat = GateHandshakeSyn::Data::Status::ErrorCode;
	data.ErrorCode = GATE_HANDSHAKE_VERIFICATION_FAILURE;

	_state = State::WriteAllAndExit;
	return data;
}

GateHandshakeSyn::Data InboundGateSession::BuildSyn()
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

	_state = State::HandshakeWaitVerificationResponse;
	_reader = new StreamReader(_fd, sizeof(int32_t));
	return syn;
}

bool InboundGateSession::VerifyPeer(
	const CowBuffer<uint8_t> buffer,
	const CowBuffer<uint8_t> signature)
{
#warning TODO: peer check.
	return true;
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
	return true;
}

bool InboundGateSession::ProcessSessionInput(CowBuffer<uint8_t> buffer)
{
#warning TODO: data processing.
	return true;
}

void InboundGateSession::InboundGateLog(String message)
{
	Log("Inbound gate from " + IPToString(_ipv4), message);
}
