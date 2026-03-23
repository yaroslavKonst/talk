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
	ServerHandshakeStorage *storage,
	EventDispatcher *dispatcher,
	const uint8_t *privateKey,
	const uint8_t *publicKey)
{
	SetInterval(10);
	SetTimestamp(GetUnixTime());

	_fd = fd;
	_state = State::WaitingSize;
	_storage = storage;
	_dispatcher = dispatcher;
	_privateKey = privateKey;
	_publicKey = publicKey;

	_user = nullptr;
	_reader = new StreamReader(fd, sizeof(int32_t) + 1);
	_writer = nullptr;

	_dispatcher->RegisterDescriptorProcessor(this);
	_dispatcher->RegisterTimeProcessor(this);

	Log("Login: New connection.");
}

ServerHandshake::~ServerHandshake()
{
	if (_fd != -1) {
		Log("Login: Connection failed.");
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
	Log("Login: Timeout.");
}

void ServerHandshake::ProcessSize(CowBuffer<uint8_t> buffer)
{
	Log("Login: Size.");

	if (buffer.Size() != sizeof(int32_t) + 1) {
		Log("Login: Protocol violation.");
		_storage->MarkSessionForRemoval(this);
		return;
	}

	_inScramblerInit = buffer[0];
	buffer = buffer.Slice(1, buffer.Size() - 1);

	_inScramblerInit = ApplyScrambler(
		buffer.Pointer(),
		buffer.Size(),
		_inScramblerInit);

	int32_t nameLength = *buffer.SwitchType<int32_t>();

	if (nameLength > 200 || nameLength <= 0) {
		Log("Login: Invalid size.");
		_storage->MarkSessionForRemoval(this);
		return;
	}

	_nameSize = buffer;
	_reader = new StreamReader(_fd, nameLength + 1);
	_state = State::WaitingSyn;
}

void ServerHandshake::ProcessSyn(CowBuffer<uint8_t> buffer)
{
	Log("Login: Syn.");

	_inScramblerInit = ApplyScrambler(
		buffer.Pointer(),
		buffer.Size(),
		_inScramblerInit);

	HandshakeSyn::Data data;
	bool parseResult = HandshakeSyn::Parse(_nameSize.Concat(buffer), data);

	if (!parseResult) {
		Log("Login: Protocol violation.");
		_storage->MarkSessionForRemoval(this);
		return;
	}

	if (!_storage->HasUser(data.Name)) {
		Log("Login: Invalid user.");
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

	InitNonce(_outES.Nonce);
	memset(_inES.Nonce, 0, NONCE_SIZE);

	_challenge = CowBuffer<uint8_t>(Handshake::ChallengeSize);
	GenerateRandomData(_challenge.Size(), _challenge.Pointer(), false);

	CowBuffer<uint8_t> encryptedChallenge = Encrypt(_challenge, _outES);

	HandshakeSynAck::Data response;
	response.Timestamp = timestamp;
	response.Challenge = encryptedChallenge.Pointer();

	CowBuffer<uint8_t> responseData = HandshakeSynAck::Build(response);

	GenerateRandomData(
		sizeof(_outScramblerInit),
		&_outScramblerInit,
		false);

	CowBuffer<uint8_t> outScrambler(1);
	outScrambler[0] = _outScramblerInit;

	_outScramblerInit = ApplyScrambler(
		responseData.Pointer(),
		responseData.Size(),
		_outScramblerInit);

	_writer = new StreamWriter(_fd, outScrambler.Concat(responseData));
	_reader = new StreamReader(_fd, HandshakeAck::Length);
}

void ServerHandshake::ProcessAck(CowBuffer<uint8_t> buffer)
{
	Log("Login: Ack.");

	if (buffer.Size() != HandshakeAck::Length) {
		Log("Login: Protocol violation.");
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
		Log("Login: Protocol violation.");
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
		Log("Login: Challenge failed.");
		_storage->MarkSessionForRemoval(this);
		return;
	}

	bool validChallenge = !crypto_verify64(
		_challenge.Pointer(),
		challenge.Pointer());

	if (!validChallenge) {
		Log("Login: Challenge failed.");
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

	Log("Login: Connecton accepted.");
}
