#include "ServerHandshake.hpp"

#include <unistd.h>
#include <sys/socket.h>

#include "User.hpp"
#include "../Protocol/HandshakeParser.hpp"
#include "../Common/Exception.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/Log.hpp"
#include "../Crypto/Crypto.hpp"

ServerHandshake::ServerHandshake(
	int fd,
	int32_t ip,
	ServerHandshakeStorage *storage,
	EventDispatcher *dispatcher,
	FailBan *failBan,
	const Crypto::X25519::PrivateKeyContainer &privateKey,
	const Crypto::X25519::PublicKeyContainer &publicKey) :
	_privateKey(privateKey),
	_publicKey(publicKey)
{
	SetInterval(10);
	SetTimestamp(GetUnixTime());

	_fd = fd;
	_ip = ip;
	_state = State::WaitingSize;
	_storage = storage;
	_dispatcher = dispatcher;
	_failBan = failBan;

	_user = nullptr;
	_reader = new StreamReader(fd, sizeof(int32_t) * 3 + 1);
	_writer = nullptr;

	_dispatcher->RegisterDescriptorProcessor(this);
	_dispatcher->RegisterTimeProcessor(this);

	HandshakeLog("", "New connection.");
}

ServerHandshake::~ServerHandshake()
{
	if (_fd != -1) {
		HandshakeLog(
			_user ? _user->GetName() : "",
			"Handshake failed.");
		shutdown(_fd, SHUT_RDWR);
		close(_fd);
		_fd = -1;

		_failBan->RecordFailure(_ip);
	}

	if (_reader) {
		delete _reader;
		_reader = nullptr;
	}

	if (_writer) {
		delete _writer;
		_writer = nullptr;
	}

	_dispatcher->UnregisterDescriptorProcessor(this);
	_dispatcher->UnregisterTimeProcessor(this);
}

bool ServerHandshake::RequestRead()
{
	return _reader;
}

bool ServerHandshake::RequestWrite()
{
	return _writer;
}

void ServerHandshake::ProcessRead()
{
	if (!_reader) {
		THROW("Reader is null.");
	}

	SetTimestamp(GetUnixTime());

	if (_writer) {
		_storage->MarkSessionForRemoval(this);
		return;
	}

	bool readSuccess = _reader->Read();

	if (!readSuccess) {
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

	if (_state == State::WaitingSize) {
		ProcessSize(buffer);
	} else if (_state == State::WaitingSyn) {
		ProcessSyn(buffer);
	} else if (_state == State::WaitingAck) {
		ProcessAck(buffer);
	}
}

void ServerHandshake::ProcessWrite()
{
	if (!_writer) {
		THROW("Writer is null.");
	}

	SetTimestamp(GetUnixTime());

	bool writeSuccess = _writer->Write();

	if (!writeSuccess) {
		delete _writer;
		_storage->MarkSessionForRemoval(this);
	}

	bool writeEnded = _writer->WritingEnd();

	if (writeEnded) {
		delete _writer;
		_writer = nullptr;
	}
}

void ServerHandshake::ProcessTimeEvent()
{
	if (_reader) {
		delete _reader;
		_reader = nullptr;
	}

	if (_writer) {
		delete _writer;
		_writer = nullptr;
	}

	_storage->MarkSessionForRemoval(this);
	HandshakeLog(_user ? _user->GetName() : "", "Timeout.");
}

void ServerHandshake::ProcessSize(CowBuffer<uint8_t> buffer)
{
	if (buffer.Size() != sizeof(int32_t) * 3 + 1) {
		HandshakeLog("", "Protocol violation.");
		_storage->MarkSessionForRemoval(this);
		return;
	}

	_inScramblerInit = buffer[0];
	buffer = buffer.Slice(1, buffer.Size() - 1);

	_inScramblerInit = Crypto::ApplyScrambler(
		buffer.Pointer(),
		buffer.Size(),
		_inScramblerInit);

	int32_t nameLength = *buffer.SwitchType<int32_t>(sizeof(int32_t) * 2);

	if (nameLength > 500 || nameLength <= 0) {
		HandshakeLog("", "Invalid size.");
		_storage->MarkSessionForRemoval(this);
		return;
	}

	_nameSize = buffer;
	_reader = new StreamReader(_fd, nameLength);
	_state = State::WaitingSyn;
}

void ServerHandshake::ProcessSyn(CowBuffer<uint8_t> buffer)
{
	_inScramblerInit = Crypto::ApplyScrambler(
		buffer.Pointer(),
		buffer.Size(),
		_inScramblerInit);

	HandshakeSyn::Data data;
	bool parseResult = HandshakeSyn::Parse(_nameSize.Concat(buffer), data);

	if (!parseResult) {
		HandshakeLog("", "Protocol violation.");
		_storage->MarkSessionForRemoval(this);
		return;
	}

	if (data.ProtocolVersion != 0) {
		HandshakeLog("", "Unsupported protocol version.");
		_storage->MarkSessionForRemoval(this);
		return;
	}

	if (data.EncryptionScheme != Crypto::X25519::SCHEME_ID) {
		HandshakeLog("", "Unsupported encryption scheme.");
		_storage->MarkSessionForRemoval(this);
		return;
	}

	if (!_storage->HasUser(data.Name)) {
		HandshakeLog(data.Name, "Invalid user.");
		_storage->MarkSessionForRemoval(this);
		return;
	}

	_user = _storage->GetUser(data.Name);
	_state = State::WaitingAck;

	int64_t timestamp = GetUnixTime();

	GenerateSessionKeys(
		_privateKey,
		_publicKey,
		_user->GetPublicKey(),
		timestamp,
		_outES.Key,
		_inES.Key);

	Crypto::X25519::InitNonce(_outES.Nonce);
	memset(_inES.Nonce, 0, Crypto::X25519::NONCE_SIZE);

	_challenge = CowBuffer<uint8_t>(Handshake::ChallengeSize);
	Crypto::GenerateRandomData(
		_challenge.Size(),
		_challenge.Pointer(),
		false);

	CowBuffer<uint8_t> encryptedChallenge = Encrypt(_challenge, _outES);

	HandshakeSynAck::Data response;
	response.Timestamp = timestamp;
	response.Challenge = encryptedChallenge.Pointer();

	CowBuffer<uint8_t> responseData = HandshakeSynAck::Build(response);

	Crypto::GenerateRandomData(
		sizeof(_outScramblerInit),
		&_outScramblerInit,
		false);

	CowBuffer<uint8_t> outScrambler(1);
	outScrambler[0] = _outScramblerInit;

	_outScramblerInit = Crypto::ApplyScrambler(
		responseData.Pointer(),
		responseData.Size(),
		_outScramblerInit);

	_writer = new StreamWriter(_fd, outScrambler.Concat(responseData));
	_reader = new StreamReader(_fd, HandshakeAck::Length);
}

void ServerHandshake::ProcessAck(CowBuffer<uint8_t> buffer)
{
	if (buffer.Size() != HandshakeAck::Length) {
		HandshakeLog(_user->GetName(), "Protocol violation.");
		_storage->MarkSessionForRemoval(this);
		return;
	}

	_inScramblerInit = Crypto::ApplyScrambler(
		buffer.Pointer(),
		buffer.Size(),
		_inScramblerInit);

	HandshakeAck::Data data;
	bool parseResult = HandshakeAck::Parse(buffer, data);

	if (!parseResult) {
		HandshakeLog(_user->GetName(), "Protocol violation.");
		_storage->MarkSessionForRemoval(this);
		return;
	}

	CowBuffer<uint8_t> encryptedChallenge(
		Handshake::EncryptedChallengeSize);
	memcpy(
		encryptedChallenge.Pointer(),
		data.Challenge,
		encryptedChallenge.Size());

	CowBuffer<uint8_t> challenge = Decrypt(encryptedChallenge, _inES);

	if (challenge.Size() != Handshake::ChallengeSize) {
		HandshakeLog(_user->GetName(), "Challenge failed.");
		_storage->MarkSessionForRemoval(this);
		return;
	}

	bool validChallenge = !crypto_verify64(
		_challenge.Pointer(),
		challenge.Pointer());

	if (!validChallenge) {
		HandshakeLog(_user->GetName(), "Challenge failed.");
		_storage->MarkSessionForRemoval(this);
		return;
	}

	HandshakeLog(_user->GetName(), "Challenge accepted.");
	HandshakeLog(_user->GetName(), "Handshake success.");

	_user->AddSession(
		_fd,
		&_outES,
		&_inES,
		_outScramblerInit,
		_inScramblerInit);

	_fd = -1;
	_storage->MarkSessionForRemoval(this);
}

void ServerHandshake::HandshakeLog(String name, String message)
{
	String text = "Login from " + IPToString(_ip);

	if (name.Length()) {
		text += ", " + name;
	}

	Log(text + ": " + message);
}
