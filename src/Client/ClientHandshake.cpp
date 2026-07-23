#include "ClientHandshake.hpp"

#include "../Protocol/HandshakeParser.hpp"
#include "../Common/Exception.hpp"

ClientHandshake::ClientHandshake(
	Root *root,
	int fd,
	String name,
	const Crypto::X25519::PrivateKeyContainer &privateKey,
	const Crypto::X25519::PublicKeyContainer &publicKey,
	const Crypto::X25519::PublicKeyContainer &serverPublicKey) :
	_privateKey(privateKey),
	_publicKey(publicKey)
{
	_root = root;

	_state = State::Error;

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

	if (_state == State::WaitingSynAckSize) {
		return ProcessSynAckSize(buffer);
	} else if (_state == State::WaitingSynAck) {
		return ProcessSynAck(buffer);
	}

	return false;
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

bool ClientHandshake::ErrorState()
{
	return _state == State::Error;
}

void ClientHandshake::InitSyn()
{
	_salt1 = CowBuffer<uint8_t>(32);
	Crypto::GenerateRandomData(_salt1.Size(), _salt1.Pointer(), false);

	CowBuffer<uint8_t> oneTimeSalt(32);
	Crypto::GenerateRandomData(
		oneTimeSalt.Size(),
		oneTimeSalt.Pointer(),
		false);

	HandshakeSyn::Data data;
	data.ProtocolVersion = 0;
	data.EncryptionScheme = Crypto::X25519::SCHEME_ID;
	data.Salt1 = _salt1;
	data.OneTimeSalt = oneTimeSalt;

	Crypto::X25519::PrivateKeyContainer oneTimePrivateKey;
	Crypto::X25519::PublicKeyContainer oneTimePublicKey;

	Crypto::X25519::GenerateEphemeralKeyPair(
		oneTimePrivateKey,
		oneTimePublicKey);

	Crypto::X25519::EncryptedStream oneTimeES;
	Crypto::X25519::InitNonce(oneTimeES.Nonce);

	Crypto::X25519::SymmetricKeyContainer unusedKey;

	bool validSessionKeys = Crypto::X25519::GenerateSessionKeys(
		oneTimePrivateKey,
		oneTimePublicKey,
		_serverPublicKey,
		oneTimeSalt,
		oneTimeES.Key,
		unusedKey,
		true);

	if (!validSessionKeys) {
		_root->Ui->Notify("Key exchange failed for user name.");
		_state = State::Error;
		return;
	}

	data.OneTimeKey = oneTimePublicKey;

	CowBuffer<uint8_t> synHeader = HandshakeSyn::Build(data);

	CowBuffer<uint8_t> synSize(sizeof(uint32_t));
	*synSize.SwitchType<uint32_t>() = synSize.Size() + synHeader.Size() +
		Crypto::X25519::CRYPTO_HEADER_SIZE + _name.Length();

	synHeader = synSize.Concat(synHeader);

	CowBuffer<uint8_t> userNameString(_name.Length());
	memcpy(userNameString.Pointer(), _name.CStr(), _name.Length());

	CowBuffer<uint8_t> encryptedUserName = Crypto::X25519::Encrypt(
		userNameString,
		oneTimeES,
		synHeader);

	CowBuffer<uint8_t> synBuffer = synHeader.Concat(encryptedUserName);

	Crypto::GenerateRandomData(
		sizeof(_outScramblerInit),
		&_outScramblerInit,
		false);

	CowBuffer<uint8_t> outScrambler(1);
	outScrambler[0] = _outScramblerInit;

	_outScramblerInit = Crypto::ApplyScrambler(
		synBuffer.Pointer(),
		synBuffer.Size(),
		_outScramblerInit);

	_writer = new StreamWriter(_fd, outScrambler.Concat(synBuffer));
	_reader = new StreamReader(_fd, sizeof(uint32_t) + 1);

	_state = State::WaitingSynAckSize;
}

bool ClientHandshake::ProcessSynAckSize(CowBuffer<uint8_t> buffer)
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

	uint32_t synAckSize = *buffer.SwitchType<uint32_t>();

	if (synAckSize > 512) {
		return false;
	}

	_synAckSize = buffer;

	_reader = new StreamReader(_fd, synAckSize - sizeof(uint32_t));

	_state = State::WaitingSynAck;
	return true;
}

bool ClientHandshake::ProcessSynAck(CowBuffer<uint8_t> buffer)
{
	_inScramblerInit = Crypto::ApplyScrambler(
		buffer.Pointer(),
		buffer.Size(),
		_inScramblerInit);

	if (buffer.Size() == sizeof(int32_t)) {
		int32_t errorStatus = *buffer.SwitchType<int32_t>();

		if (errorStatus ==
			HANDSHAKE_RESPONSE_UNSUPPORTED_PROTOCOL_VERSION)
		{
			_root->Ui->Notify("Unsupported protocol version.");
		} else if (errorStatus ==
			HANDSHAKE_RESPONSE_UNSUPPORTED_ENCRYPTION_SCHEME)
		{
			_root->Ui->Notify("Unsupported encryption scheme.");
		} else {
			_root->Ui->Notify(
				"Unknown error code from server: " +
				ToString(errorStatus) + ".");
		}

		return false;
	}

	bool validSessionKeys = Crypto::X25519::GenerateSessionKeys(
		_privateKey,
		_publicKey,
		_serverPublicKey,
		_salt1,
		_inES.Key,
		_outES.Key,
		true);

	if (!validSessionKeys) {
		_root->Ui->Notify("Key exchange failed for handshake keys.");
		return false;
	}

	Crypto::X25519::InitNonce(_outES.Nonce);
	memset(_inES.Nonce, 0, Crypto::X25519::NONCE_SIZE);

	buffer = Crypto::X25519::Decrypt(buffer, _inES, _synAckSize);

	if (!buffer.Size()) {
		_root->Ui->Notify("Failed to decrypt SynAck.");
		return false;
	}

	HandshakeSynAck::Data data;
	bool parseResult = HandshakeSynAck::Parse(buffer, data);

	if (!parseResult) {
		_root->Ui->Notify("Failed to parse SynAck.");
		return false;
	}

	if (data.ProtocolVersion != 0 ||
		data.EncryptionScheme != Crypto::X25519::SCHEME_ID)
	{
		_root->Ui->Notify("Invalid protocol parameters in SynAck.");
		return false;
	}

	Crypto::X25519::PrivateKeyContainer ephemeralPrivateKey;
	Crypto::X25519::PublicKeyContainer ephemeralPublicKey;

	Crypto::X25519::GenerateEphemeralKeyPair(
		ephemeralPrivateKey,
		ephemeralPublicKey);

	HandshakeAck::Data outData;
	outData.Challenge = data.Challenge;
	outData.ClientSessionPublicKey = ephemeralPublicKey;

	CowBuffer<uint8_t> outBuffer = HandshakeAck::Build(outData);

	outBuffer = Encrypt(outBuffer, _outES);

	_outScramblerInit = Crypto::ApplyScrambler(
		outBuffer.Pointer(),
		outBuffer.Size(),
		_outScramblerInit);

	_writer = new StreamWriter(_fd, outBuffer);

	validSessionKeys = Crypto::X25519::GenerateSessionKeys(
		ephemeralPrivateKey,
		ephemeralPublicKey,
		data.ServerSessionPublicKey,
		_salt1.Concat(data.Salt2),
		_inES.Key,
		_outES.Key,
		true);

	if (!validSessionKeys) {
		_root->Ui->Notify("Key exchange failed for session keys.");
		return false;
	}

	Crypto::X25519::InitNonce(_outES.Nonce);
	memset(_inES.Nonce, 0, Crypto::X25519::NONCE_SIZE);

	_state = State::Ready;
	return true;
}
