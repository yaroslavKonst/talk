#include "ServerHandshake.hpp"

#include <unistd.h>
#include <sys/socket.h>

#include "User.hpp"
#include "../Protocol/CommonParserConstants.hpp"
#include "../Common/Exception.hpp"
#include "../Common/Endianness.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/Log.hpp"
#include "../Crypto/Crypto.hpp"

ServerHandshake::ServerHandshake(
	int fd,
	IPAddress ip,
	ServerHandshakeStorage *storage,
	EventDispatcher *dispatcher,
	FailBan *failBan,
	const Crypto::X25519::PrivateKeyContainer &privateKey,
	const Crypto::X25519::PublicKeyContainer &publicKey) :
	_privateKey(privateKey),
	_publicKey(publicKey)
{
	// Timeout 10 seconds.
	SetInterval(10000);
	SetTimestamp(GetMonotonicMillisecondTime());

	_fd = fd;
	_ip = ip;
	_state = State::WaitingSynSize;
	_storage = storage;
	_dispatcher = dispatcher;
	_failBan = failBan;

	_user = nullptr;
	_reader = new StreamReader(fd, sizeof(int32_t) + 1);
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

	SetTimestamp(GetMonotonicMillisecondTime());

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

	if (_state == State::WaitingSynSize) {
		ProcessSynSize(buffer);
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

	SetTimestamp(GetMonotonicMillisecondTime());

	bool writeSuccess = _writer->Write();

	if (!writeSuccess) {
		delete _writer;
		_writer = nullptr;
		_storage->MarkSessionForRemoval(this);
		return;
	}

	bool writeEnded = _writer->WritingEnd();

	if (writeEnded) {
		delete _writer;
		_writer = nullptr;

		if (_state == State::SendAllAndExit) {
			_storage->MarkSessionForRemoval(this);
		}
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

void ServerHandshake::ProcessSynSize(CowBuffer<uint8_t> buffer)
{
	if (buffer.Size() != sizeof(int32_t) + 1) {
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

	uint32_t synLength = SetProtoEndian(*buffer.SwitchType<uint32_t>());

	if (synLength > CommonParserConstants::SmallDatagramSize || !synLength)
	{
		HandshakeLog("", "Invalid Syn size.");
		_storage->MarkSessionForRemoval(this);
		return;
	}

	_synSize = buffer;
	_reader = new StreamReader(_fd, synLength - sizeof(uint32_t));
	_state = State::WaitingSyn;
}

void ServerHandshake::ProcessSyn(CowBuffer<uint8_t> buffer)
{
	_inScramblerInit = Crypto::ApplyScrambler(
		buffer.Pointer(),
		buffer.Size(),
		_inScramblerInit);

	HandshakeSyn::Data data;
	bool parseResult = HandshakeSyn::Parse(buffer, data);

	if (!parseResult) {
		HandshakeLog("", "Syn protocol violation.");
		_storage->MarkSessionForRemoval(this);
		return;
	}

	parseResult = CheckProtocolVersion(data);

	if (!parseResult) {
		return;
	}

	parseResult = CheckEncryptionScheme(data);

	if (!parseResult) {
		return;
	}

	buffer = _synSize.Concat(buffer);

	String userName = DecryptUserNameFromSyn(data, buffer);

	if (!userName.Length()) {
		return;
	}

	if (!_storage->HasUser(userName)) {
		HandshakeLog(userName, "Invalid user.");
		_storage->MarkSessionForRemoval(this);
		return;
	}

	_user = _storage->GetUser(userName);
	_state = State::WaitingAck;
	_salt1 = data.Salt1;

	GenerateEphemeralKeys();

	if (!GenerateHandshakeKeys()) {
		HandshakeLog(
			userName,
			"Key exchange failed for handshake keys.");
		_storage->MarkSessionForRemoval(this);
		return;
	}

	HandshakeSynAck::Data outData;
	outData.ProtocolVersion = data.ProtocolVersion;
	outData.EncryptionScheme = data.EncryptionScheme;

	outData.Challenge = CowBuffer<uint8_t>(Handshake::ChallengeSize);
	Crypto::GenerateRandomData(
		outData.Challenge.Size(),
		outData.Challenge.Pointer(),
		false);

	_challenge = outData.Challenge;

	outData.ServerSessionPublicKey = _ephemeralPublicKey;

	_salt2 = CowBuffer<uint8_t>(Handshake::SaltSize);
	Crypto::GenerateRandomData(
		_salt2.Size(),
		_salt2.Pointer(),
		false);

	outData.Salt2 = _salt2;

	CowBuffer<uint8_t> outBuffer = HandshakeSynAck::Build(outData);

	CowBuffer<uint8_t> outSizeBuffer(sizeof(uint32_t));
	*outSizeBuffer.SwitchType<uint32_t>() = SetProtoEndian<uint32_t>(
		outBuffer.Size() +
		sizeof(uint32_t) + Crypto::X25519::CRYPTO_HEADER_SIZE);

	outBuffer = Encrypt(outBuffer, _outES, outSizeBuffer);

	outBuffer = outSizeBuffer.Concat(outBuffer);

	Crypto::GenerateRandomData(
		sizeof(_outScramblerInit),
		&_outScramblerInit,
		false);

	CowBuffer<uint8_t> outScrambler(1);
	outScrambler[0] = _outScramblerInit;

	_outScramblerInit = Crypto::ApplyScrambler(
		outBuffer.Pointer(),
		outBuffer.Size(),
		_outScramblerInit);

	_writer = new StreamWriter(_fd, outScrambler.Concat(outBuffer));
	_reader = new StreamReader(
		_fd,
		HandshakeAck::Length + Crypto::X25519::CRYPTO_HEADER_SIZE);
}

bool ServerHandshake::CheckProtocolVersion(const HandshakeSyn::Data &data)
{
	if (data.ProtocolVersion == 0) {
		return true;
	}

	HandshakeLog("", "Unsupported protocol version.");

	CowBuffer<uint8_t> response(sizeof(int32_t) * 2);

	*response.SwitchType<uint32_t>() =
		SetProtoEndian<uint32_t>(response.Size());
	*response.SwitchType<int32_t>(sizeof(uint32_t)) =
		SetProtoEndian<int32_t>(
			HANDSHAKE_RESPONSE_UNSUPPORTED_PROTOCOL_VERSION);

	_writer = new StreamWriter(_fd, Crypto::ApplyScrambler(response));
	_state = State::SendAllAndExit;

	return false;
}

bool ServerHandshake::CheckEncryptionScheme(const HandshakeSyn::Data &data)
{
	if (data.EncryptionScheme == Crypto::X25519::SCHEME_ID) {
		return true;
	}

	HandshakeLog("", "Unsupported encryption scheme.");

	CowBuffer<uint8_t> response(sizeof(int32_t) * 2);

	*response.SwitchType<uint32_t>() =
		SetProtoEndian<uint32_t>(response.Size());
	*response.SwitchType<int32_t>(sizeof(uint32_t)) =
		SetProtoEndian<int32_t>(
			HANDSHAKE_RESPONSE_UNSUPPORTED_ENCRYPTION_SCHEME);

	_writer = new StreamWriter(_fd, Crypto::ApplyScrambler(response));
	_state = State::SendAllAndExit;

	return false;
}

String ServerHandshake::DecryptUserNameFromSyn(
	const HandshakeSyn::Data &data,
	const CowBuffer<uint8_t> buffer)
{
	Crypto::X25519::SymmetricKeyContainer userNameKey;
	Crypto::X25519::SymmetricKeyContainer unusedKey;

	bool validSessionKeys = Crypto::X25519::GenerateSessionKeys(
		_privateKey,
		_publicKey,
		data.OneTimeKey,
		data.OneTimeSalt,
		userNameKey,
		unusedKey,
		false);

	if (!validSessionKeys) {
		HandshakeLog("", "Key exchange failed for user name.");
		_storage->MarkSessionForRemoval(this);
		return "";
	}

	Crypto::X25519::EncryptedStream userNameStream;
	userNameStream.Key = userNameKey;
	memset(userNameStream.Nonce, 0, Crypto::X25519::NONCE_SIZE);

	CowBuffer<uint8_t> userNameString = Crypto::X25519::Decrypt(
		data.EncryptedName,
		userNameStream,
		buffer.Slice(
			0,
			sizeof(int32_t) * 3 +
			Handshake::SaltSize * 2 +
			Crypto::X25519::KEY_SIZE));

	if (!userNameString.Size()) {
		HandshakeLog("", "User name decryption failed.");
		_storage->MarkSessionForRemoval(this);
		return "";
	}

	String userName(
		userNameString.SwitchType<char>(),
		userNameString.Size());

	return userName;
}

void ServerHandshake::GenerateEphemeralKeys()
{
	Crypto::X25519::GenerateEphemeralKeyPair(
		_ephemeralPrivateKey,
		_ephemeralPublicKey);
}

bool ServerHandshake::GenerateHandshakeKeys()
{
	bool validSessionKeys = GenerateSessionKeys(
		_privateKey,
		_publicKey,
		_user->GetPublicKey(),
		_salt1,
		_outES.Key,
		_inES.Key,
		false);

	if (!validSessionKeys) {
		return false;
	}

	Crypto::X25519::InitNonce(_outES.Nonce);
	memset(_inES.Nonce, 0, Crypto::X25519::NONCE_SIZE);

	return true;
}

void ServerHandshake::ProcessAck(CowBuffer<uint8_t> buffer)
{
	if (buffer.Size() !=
		HandshakeAck::Length + Crypto::X25519::CRYPTO_HEADER_SIZE)
	{
		HandshakeLog(_user->GetName(), "Protocol violation.");
		_storage->MarkSessionForRemoval(this);
		return;
	}

	_inScramblerInit = Crypto::ApplyScrambler(
		buffer.Pointer(),
		buffer.Size(),
		_inScramblerInit);

	buffer = Decrypt(buffer, _inES);

	if (!buffer.Size()) {
		HandshakeLog(_user->GetName(), "Challenge decryption failure.");
		_storage->MarkSessionForRemoval(this);
		return;
	}

	HandshakeAck::Data data;
	bool parseResult = HandshakeAck::Parse(buffer, data);

	if (!parseResult) {
		HandshakeLog(_user->GetName(), "Ack protocol violation.");
		_storage->MarkSessionForRemoval(this);
		return;
	}

	bool validChallenge = !crypto_verify64(
		_challenge.Pointer(),
		data.Challenge.Pointer());

	if (!validChallenge) {
		HandshakeLog(_user->GetName(), "Challenge failed.");
		_storage->MarkSessionForRemoval(this);
		return;
	}

	HandshakeLog(_user->GetName(), "Challenge accepted.");

	Crypto::X25519::EncryptedStream outStream;
	Crypto::X25519::EncryptedStream inStream;

	bool validSessionKeys = Crypto::X25519::GenerateSessionKeys(
		_ephemeralPrivateKey,
		_ephemeralPublicKey,
		data.ClientSessionPublicKey,
		_salt1.Concat(_salt2),
		outStream.Key,
		inStream.Key,
		false);

	if (!validSessionKeys) {
		HandshakeLog(
			_user->GetName(),
			"Key exchange failed for session keys.");
		_storage->MarkSessionForRemoval(this);
		return;
	}

	Crypto::X25519::InitNonce(outStream.Nonce);
	memset(inStream.Nonce, 0, Crypto::X25519::NONCE_SIZE);

	HandshakeLog(_user->GetName(), "Handshake success.");

	_user->AddSession(
		_fd,
		outStream,
		inStream,
		_outScramblerInit,
		_inScramblerInit);

	_fd = -1;
	_storage->MarkSessionForRemoval(this);
}

void ServerHandshake::HandshakeLog(String name, String message)
{
	String text = "Login from " + _ip.ToString();

	if (name.Length()) {
		text += ", " + name;
	}

	Log(LogLevel::Info, text, message);
}
