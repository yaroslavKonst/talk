#include "VoiceChat.hpp"

#include "../Protocol/StreamParser.hpp"
#include "../Common/Endianness.hpp"

VoiceChat::VoiceChat(Root *root)
{
	_root = root;

	_state = State::Closed;
}

VoiceChat::~VoiceChat()
{
	StreamEnd();
}

VoiceChat::State VoiceChat::GetState()
{
	return _state;
}

String VoiceChat::GetPeerName()
{
	return _peerName;
}

Crypto::X25519::PublicKeyContainer VoiceChat::GetPeerPublicKey()
{
	return _peerPublicKey;
}

void VoiceChat::InitCall(
	String peerName,
	const Crypto::X25519::PublicKeyContainer &peerPublicKey)
{
	if (_state != State::Closed) {
		_root->Ui->Notify("Call is already active.");
		return;
	}

	_peerName = peerName;
	_peerPublicKey = peerPublicKey;

	StreamHandshake::InitRequest request;
	request.Source =
		_root->Conf->GetName() + "@" + _root->Conf->GetHostName();
	request.SourceKey = *_root->PublicKey;
	request.Destination = peerName;
	request.DestinationKey = peerPublicKey;

	_salt1.Resize(StreamHandshake::SaltSize);
	Crypto::GenerateRandomData(
		_salt1.Size(),
		_salt1.Pointer(),
		false);

	request.Salt = _salt1;

	_challenge.Resize(StreamHandshake::ChallengeSize);
	Crypto::GenerateRandomData(
		_challenge.Size(),
		_challenge.Pointer(),
		false);

	bool keyExchangeSuccess = Crypto::X25519::GenerateSessionKeys(
		*_root->PrivateKey,
		*_root->PublicKey,
		_peerPublicKey,
		_salt1,
		_initOutES.Key,
		_initInES.Key,
		false);

	if (!keyExchangeSuccess) {
		_root->Ui->Notify("Key exchange for call failed.");
		return;
	}

	Crypto::X25519::InitNonce(_initOutES.Nonce);
	memset(_initInES.Nonce, 0, Crypto::X25519::NONCE_SIZE);

	Crypto::X25519::GenerateEphemeralKeyPair(
		_ephemeralPrivateKey,
		_ephemeralPublicKey);

	CowBuffer<uint8_t> requestBuffer = BuildInitRequest(request);

	StreamHandshake::ProtectedInitRequest protectedInitRequest;
	protectedInitRequest.EphemeralKey = _ephemeralPublicKey;
	protectedInitRequest.Challenge = _challenge;

	request.ProtectedPart =
		Crypto::X25519::Encrypt(
			BuildProtectedInitRequest(protectedInitRequest),
			_initOutES,
			requestBuffer);

	requestBuffer = BuildInitRequest(request);

	bool sendSuccess = _root->Network->SendStreamInit(requestBuffer);

	if (!sendSuccess) {
		_root->Ui->Notify("Failed to start call. No connection.");
		return;
	}

	_state = State::InitSent;
}

void VoiceChat::EndCall()
{
	_root->Network->SendStreamEnd();
	StreamEnd();
}

void VoiceChat::StreamEnd()
{
	if (_state == State::Closed) {
		return;
	}

	_state = State::Closed;
	_root->Ui->Redraw();
}

void VoiceChat::ProcessInitResponse(int32_t status)
{
	if (_state != State::InitSent) {
		EndCall();
		return;
	}

	if (status == STREAM_INIT_RESPONSE_WAITING_FOR_ANSWER) {
		_state = State::WaitingForPeerAnswer;
		_root->Ui->Redraw();
		return;
	}

	StreamEnd();

	switch (status) {
	case STREAM_INIT_RESPONSE_ERROR:
		_root->Ui->Notify("Call failed.");
		break;
	case STREAM_INIT_RESPONSE_YOU_ARE_IN_CALL:
		_root->Ui->Notify(
			"You are already in a call from another device.");
		break;
	case STREAM_INIT_RESPONSE_SERVER_OFFLINE:
		_root->Ui->Notify("Requested server is offline.");
		break;
	case STREAM_INIT_RESPONSE_USER_OFFLINE:
		_root->Ui->Notify("Requested user is offline.");
		break;
	case STREAM_INIT_RESPONSE_USER_BUSY:
		_root->Ui->Notify("Requested user is in another call.");
		break;
	case STREAM_INIT_RESPONSE_USER_NONEXISTENT:
		_root->Ui->Notify("Requested user does not exist.");
		break;
	case STREAM_INIT_RESPONSE_INVALID_DESTINATION_KEY:
		_root->Ui->Notify("Peer key is rejected.");
		break;
	case STREAM_INIT_RESPONSE_CALL_PROHIBITED:
		_root->Ui->Notify("Requested user does not allow calls.");
		break;
	case STREAM_INIT_RESPONSE_YOU_ARE_BANNED:
		_root->Ui->Notify("You are banned.");
		break;
	case STREAM_INIT_RESPONSE_YOUR_KEY_IS_BANNED:
		_root->Ui->Notify("Your key is banned.");
		break;
	case STREAM_INIT_RESPONSE_PARSING_FAILURE:
		_root->Ui->Notify("Peer server reported malformed request.");
		break;
	default:
		_root->Ui->Notify("Unknown call failure code.");
		break;
	}
}

void VoiceChat::ProcessInit(const CowBuffer<uint8_t> buffer)
{
	StreamHandshake::InitRequest request;
	bool parseResult = ParseInitRequest(buffer, request);

	if (!parseResult || _state != State::Closed) {
		EndCall();
		return;
	}

	_peerName = request.Source;
	_peerPublicKey = request.SourceKey;

	if (request.Destination !=
		_root->Conf->GetName() + "@" + _root->Conf->GetHostName())
	{
		_root->Network->SendStreamEnd();
		return;
	}

	if (crypto_verify32(request.DestinationKey.Key, _root->PublicKey->Key))
	{
		_root->Network->SendStreamEnd();
		return;
	}

	_salt1 = request.Salt;

	bool keyExchangeSuccess = Crypto::X25519::GenerateSessionKeys(
		*_root->PrivateKey,
		*_root->PublicKey,
		_peerPublicKey,
		_salt1,
		_initInES.Key,
		_initOutES.Key,
		true);

	if (!keyExchangeSuccess) {
		_root->Network->SendStreamEnd();
		return;
	}

	Crypto::X25519::InitNonce(_initOutES.Nonce);
	memset(_initInES.Nonce, 0, Crypto::X25519::NONCE_SIZE);

	CowBuffer<uint8_t> protectedRequestBuffer = Crypto::X25519::Decrypt(
		request.ProtectedPart,
		_initInES,
		buffer.Slice(0, buffer.Size() -
			request.ProtectedPart.Size()));

	if (!protectedRequestBuffer.Size()) {
		_root->Network->SendStreamEnd();
		return;
	}

	StreamHandshake::ProtectedInitRequest protectedRequest;
	parseResult = ParseProtectedInitRequest(
		protectedRequestBuffer,
		protectedRequest);

	if (!parseResult) {
		_root->Network->SendStreamEnd();
		return;
	}

	_peerEphemeralPublicKey = protectedRequest.EphemeralKey;
	_challenge = protectedRequest.Challenge;

	_state = State::WaitingForUserAnswer;
	_root->Ui->Redraw();
}

void VoiceChat::RespondToInboundCall(bool answer)
{
	if (_state != State::WaitingForUserAnswer) {
		EndCall();
		return;
	}

	StreamHandshake::ProtectedPeerResponse protectedResponse;

	_salt2.Resize(StreamHandshake::SaltSize);

	Crypto::GenerateRandomData(_salt2.Size(), _salt2.Pointer(), false);

	if (answer) {
		protectedResponse.ResponseCode =
			STREAM_PEER_RESPONSE_ACCEPT;
	} else {
		protectedResponse.ResponseCode =
			STREAM_PEER_RESPONSE_DECLINE;
	}

	protectedResponse.Salt = _salt2;

	if (!answer) {
		memset(
			protectedResponse.EphemeralKey.Key,
			0,
			Crypto::X25519::KEY_SIZE);
	} else {
		Crypto::X25519::GenerateEphemeralKeyPair(
			_ephemeralPrivateKey,
			_ephemeralPublicKey);

		protectedResponse.EphemeralKey = _ephemeralPublicKey;

		bool validSessionKey = Crypto::X25519::GenerateSessionKeys(
			_ephemeralPrivateKey,
			_ephemeralPublicKey,
			_peerEphemeralPublicKey,
			_salt1.Concat(_salt2),
			_inES.Key,
			_outES.Key,
			true);

		if (!validSessionKey) {
			_root->Ui->Notify("Ephemeral key exchange failed.");
			EndCall();
			return;
		}

		Crypto::X25519::InitNonce(_outES.Nonce);
		memset(_inES.Nonce, 0, Crypto::X25519::NONCE_SIZE);
	}

	protectedResponse.Challenge = _challenge;

	CowBuffer<uint8_t> buffer = BuildProtectedPeerResponse(
		protectedResponse);

	buffer = Crypto::X25519::Encrypt(buffer, _initOutES);

	bool success = _root->Network->SendStreamRequest(buffer);

	if (!success) {
		StreamEnd();
		_root->Ui->Notify("Failed to answer call, network failure.");
		return;
	}

	if (answer) {
		_state = State::ActiveSession;
		StartMainSession();
	} else {
		EndCall();
	}
}

void VoiceChat::ProcessPeerResponse(const CowBuffer<uint8_t> buffer)
{
	if (_state != State::WaitingForPeerAnswer) {
		EndCall();
		return;
	}

	CowBuffer<uint8_t> decryptedBuffer =
		Crypto::X25519::Decrypt(buffer, _initInES);

	if (!decryptedBuffer.Size()) {
		EndCall();
		return;
	}

	StreamHandshake::ProtectedPeerResponse response;
	bool parseResult =
		ParseProtectedPeerResponse(decryptedBuffer, response);

	if (!parseResult) {
		EndCall();
		return;
	}

	if (crypto_verify64(
		response.Challenge.Pointer(),
		_challenge.Pointer()))
	{
		EndCall();
		return;
	}

	if (response.ResponseCode != STREAM_PEER_RESPONSE_ACCEPT) {
		_root->Ui->Notify("Call declined.");
		EndCall();
		return;
	}

	_salt2 = response.Salt;
	_peerEphemeralPublicKey = response.EphemeralKey;

	bool validSessionKey = Crypto::X25519::GenerateSessionKeys(
		_ephemeralPrivateKey,
		_ephemeralPublicKey,
		_peerEphemeralPublicKey,
		_salt1.Concat(_salt2),
		_outES.Key,
		_inES.Key,
		false);

	if (!validSessionKey) {
		_root->Ui->Notify("Ephemeral key exchange failed.");
		EndCall();
		return;
	}

	Crypto::X25519::InitNonce(_outES.Nonce);
	memset(_inES.Nonce, 0, Crypto::X25519::NONCE_SIZE);

	_state = State::ActiveSession;
	StartMainSession();
	_root->Ui->Redraw();
}

void VoiceChat::StartMainSession()
{
}
