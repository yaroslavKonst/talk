#include "ClientHandshake.hpp"

#include "../Protocol/HandshakeParser.hpp"
#include "../Common/Exception.hpp"

ClientHandshake::ClientHandshake(
	int fd,
	String name,
	const Crypto::X25519::PrivateKeyContainer &privateKey,
	const Crypto::X25519::PublicKeyContainer &publicKey,
	const Crypto::X25519::PublicKeyContainer &serverPublicKey) :
	_privateKey(privateKey),
	_publicKey(publicKey)
{
	_fd = fd;
	_name = name;

	_serverPublicKey = serverPublicKey;

	_reader = nullptr;
	_writer = nullptr;

	InitSyn();
}

ClientHandshake::~ClientHandshake()
{
	if (_reader) {
		delete _reader;
		_reader = nullptr;
	}

	if (_writer) {
		delete _writer;
		_writer = nullptr;
	}
}

bool ClientHandshake::RequestRead()
{
	return _reader;
}

bool ClientHandshake::RequestWrite()
{
	return _writer;
}

bool ClientHandshake::ProcessRead()
{
	if (!_reader) {
		THROW("Reader is null");
	}

	if (_writer) {
		return false;
	}

	bool readSuccess = _reader->Read();

	if (!readSuccess) {
		delete _reader;
		_reader = nullptr;
		return false;
	}

	bool readComplete = _reader->ReadingEnd();

	if (!readComplete) {
		return true;
	}

	CowBuffer<uint8_t> buffer = _reader->GetBuffer();
	delete _reader;
	_reader = nullptr;

	return ProcessSynAck(buffer);
}

bool ClientHandshake::ProcessWrite()
{
	if (!_writer) {
		THROW("Writer is null");
	}

	bool writeSuccess = _writer->Write();

	if (!writeSuccess) {
		return false;
	}

	bool writeEnded = _writer->WritingEnd();

	if (writeEnded) {
		delete _writer;
		_writer = nullptr;
	}

	return true;
}

bool ClientHandshake::ConnectionSuccessful()
{
	return _state == State::Ready && !_writer && !_reader;
}

void ClientHandshake::InitSyn()
{
	HandshakeSyn::Data data;
	data.ProtocolVersion = 0;
	data.EncryptionScheme = Crypto::X25519::SCHEME_ID;
	data.Name = _name;

	CowBuffer<uint8_t> buffer = HandshakeSyn::Build(data);

	Crypto::GenerateRandomData(
		sizeof(_outScramblerInit),
		&_outScramblerInit,
		false);

	CowBuffer<uint8_t> outScrambler(1);
	outScrambler[0] = _outScramblerInit;

	_outScramblerInit = Crypto::ApplyScrambler(
		buffer.Pointer(),
		buffer.Size(),
		_outScramblerInit);

	_writer = new StreamWriter(_fd, outScrambler.Concat(buffer));
	_reader = new StreamReader(_fd, HandshakeSynAck::Length + 1);

	_state = State::WaitingSynAck;
}

bool ClientHandshake::ProcessSynAck(CowBuffer<uint8_t> buffer)
{
	if (buffer.Size() != HandshakeSynAck::Length + 1) {
		return false;
	}

	_inScramblerInit = buffer[0];

	buffer = buffer.Slice(1, buffer.Size() - 1);
	_inScramblerInit = Crypto::ApplyScrambler(
		buffer.Pointer(),
		buffer.Size(),
		_inScramblerInit);

	HandshakeSynAck::Data data;
	bool parseResult = HandshakeSynAck::Parse(buffer, data);

	if (!parseResult) {
		return false;
	}

	Crypto::X25519::GenerateSessionKeys(
		_privateKey,
		_publicKey,
		_serverPublicKey,
		data.Timestamp,
		_inES.Key,
		_outES.Key,
		true);

	Crypto::X25519::InitNonce(_outES.Nonce);
	memset(_inES.Nonce, 0, Crypto::X25519::NONCE_SIZE);

	CowBuffer<uint8_t> encryptedChallenge(
		Handshake::EncryptedChallengeSize);
	memcpy(
		encryptedChallenge.Pointer(),
		data.Challenge,
		encryptedChallenge.Size());

	CowBuffer<uint8_t> challenge = Decrypt(encryptedChallenge, _inES);

	if (challenge.Size() != Handshake::ChallengeSize) {
		return false;
	}

	challenge = Encrypt(challenge, _outES);

	HandshakeAck::Data response;
	response.Challenge = challenge.Pointer();

	CowBuffer<uint8_t> responseData = HandshakeAck::Build(response);

	_outScramblerInit = Crypto::ApplyScrambler(
		responseData.Pointer(),
		responseData.Size(),
		_outScramblerInit);

	_writer = new StreamWriter(_fd, responseData);

	_state = State::Ready;
	return true;
}
