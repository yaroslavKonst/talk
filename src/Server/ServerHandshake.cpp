#include "ServerHandshake.hpp"

#include <unistd.h>
#include <sys/socket.h>

#include "User.hpp"
#include "../Protocol/HandshakeParser.hpp"
#include "../Common/Exception.hpp"
#include "../Common/UnixTime.hpp"
#include "../Crypto/Crypto.hpp"

ServerHandshake::ServerHandshake(
	int fd,
	ServerHandshakeStorage *storage,
	EventDispatcher *dispatcher,
	const uint8_t *privateKey,
	const uint8_t *publicKey)
{
	SetInterval(10);
	SetTimestamp(GetUnixTime());

	_fd = fd;
	_state = State::WaitingSyn;
	_storage = storage;
	_dispatcher = dispatcher;
	_privateKey = privateKey;
	_publicKey = publicKey;

	_user = nullptr;
	_reader = new StreamReader(fd, HandshakeSyn::Length + 1);
	_writer = nullptr;

	_dispatcher->RegisterDescriptorProcessor(this);
	_dispatcher->RegisterTimeProcessor(this);
}

ServerHandshake::~ServerHandshake()
{
	if (_fd != -1) {
		shutdown(_fd, SHUT_RDWR);
		close(_fd);
		_fd = -1;
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

	if (_state == State::WaitingSyn) {
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
}

void ServerHandshake::ProcessSyn(CowBuffer<uint8_t> buffer)
{
	if (buffer.Size() != HandshakeSyn::Length + 1) {
		_storage->MarkSessionForRemoval(this);
		return;
	}

	_inScramblerInit = buffer[0];
	buffer = RemoveScrambler(buffer);

	HandshakeSyn::Data data;
	bool parseResult = HandshakeSyn::Parse(buffer, data);

	if (!parseResult) {
		_storage->MarkSessionForRemoval(this);
		return;
	}

	if (!_storage->HasUser(data.Key)) {
		_storage->MarkSessionForRemoval(this);
		return;
	}

	_user = _storage->GetUser(data.Key);
	_state = State::WaitingAck;

	int64_t timestamp = GetUnixTime();

	GenerateSessionKeys(
		_privateKey,
		_publicKey,
		_user->GetPublicKey(),
		timestamp,
		_outES.Key,
		_inES.Key);

	InitNonce(_outES.Nonce);
	memset(_inES.Nonce, 0, NONCE_SIZE);

	_challenge = CowBuffer<uint8_t>(Handshake::ChallengeSize);
	GenerateRandomData(_challenge.Size(), _challenge.Pointer(), false);

	CowBuffer<uint8_t> encryptedChallenge = Encrypt(_challenge, _outES);

	HandshakeSynAck::Data response;
	response.Timestamp = timestamp;
	response.Challenge = encryptedChallenge.Pointer();

	CowBuffer<uint8_t> responseData = HandshakeSynAck::Build(response);

	responseData = ApplyScrambler(responseData);
	_outScramblerInit = responseData[0];

	_writer = new StreamWriter(_fd, responseData);
	_reader = new StreamReader(_fd, HandshakeAck::Length);
}

void ServerHandshake::ProcessAck(CowBuffer<uint8_t> buffer)
{
	if (buffer.Size() != HandshakeAck::Length) {
		_storage->MarkSessionForRemoval(this);
		return;
	}

	_inScramblerInit = ApplyScrambler(
		buffer.Pointer(),
		buffer.Size(),
		_inScramblerInit);

	HandshakeAck::Data data;
	bool parseResult = HandshakeAck::Parse(buffer, data);

	if (!parseResult) {
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
		_storage->MarkSessionForRemoval(this);
		return;
	}

	bool validChallenge = !crypto_verify64(
		_challenge.Pointer(),
		challenge.Pointer());

	if (!validChallenge) {
		_storage->MarkSessionForRemoval(this);
		return;
	}

	_user->AddSession(
		_fd,
		&_outES,
		&_inES,
		_outScramblerInit,
		_inScramblerInit);

	_fd = -1;
	_storage->MarkSessionForRemoval(this);
}
