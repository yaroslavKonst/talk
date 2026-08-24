#ifndef _VOICE_CHAT_HPP
#define _VOICE_CHAT_HPP

#include "Root.hpp"
#include "../Audio/Audio.hpp"

class VoiceChat : public VoiceEventProcessor
{
public:
	VoiceChat(Root *root);
	~VoiceChat();

	State GetState() override;
	String GetPeerName() override;
	Crypto::X25519::PublicKeyContainer GetPeerPublicKey() override;

	void InitCall(
		String peerName,
		const Crypto::X25519::PublicKeyContainer &peerPublicKey)
		override;
	void EndCall() override;

	void StreamEnd() override;

	void ProcessInitResponse(int32_t status) override;

	// Inbound call.
	void ProcessInit(const CowBuffer<uint8_t> buffer) override;
	void RespondToInboundCall(bool answer) override;

	// Peer response for outbound call.
	void ProcessPeerResponse(const CowBuffer<uint8_t> buffer) override;

private:
	Root *_root;

	State _state;

	String _peerName;
	Crypto::X25519::PublicKeyContainer _peerPublicKey;
	Crypto::X25519::PublicKeyContainer _peerEphemeralPublicKey;

	Crypto::X25519::EncryptedStream _initInES;
	Crypto::X25519::EncryptedStream _initOutES;

	CowBuffer<uint8_t> _salt1;
	CowBuffer<uint8_t> _salt2;
	CowBuffer<uint8_t> _challenge;

	Crypto::X25519::PrivateKeyContainer _ephemeralPrivateKey;
	Crypto::X25519::PublicKeyContainer _ephemeralPublicKey;

	Crypto::X25519::EncryptedStream _inES;
	Crypto::X25519::EncryptedStream _outES;

	void StartMainSession();

	enum class LineType
	{
		Audio,
		Video,
		Custom
	};

	struct OutboundLineBase
	{
		LineType Type;
	};

	struct OutboundAudioLine : public OutboundLineBase
	{
	};

	CowBuffer<OutboundLineBase*> _outboundLines;

	struct InboundLineBase
	{
		LineType Type;
	};

	struct InboundAudioLine : public InboundLineBase
	{
	};

	CowBuffer<InboundLineBase*> _inboundLines;

	void RequestNewLine(LineType type);
	void CloseLine(uint32_t index);
};

#endif
