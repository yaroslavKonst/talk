#ifndef _SERVER_HANDSHAKE_HPP
#define _SERVER_HANDSHAKE_HPP

#include "User.hpp"
#include "FailBan.hpp"
#include "../Protocol/HandshakeParser.hpp"
#include "../Common/EventDispatcher.hpp"
#include "../Common/StreamReader.hpp"
#include "../Common/StreamWriter.hpp"
#include "../Crypto/Crypto.hpp"

class ServerHandshakeStorage;

class ServerHandshake :
	public DescriptorEventProcessor,
	public TimeEventProcessor
{
public:
	ServerHandshake(
		int fd,
		int32_t ip,
		ServerHandshakeStorage *storage,
		EventDispatcher *dispatcher,
		FailBan *failBan,
		const Crypto::X25519::PrivateKeyContainer &privateKey,
		const Crypto::X25519::PublicKeyContainer &publicKey);
	~ServerHandshake();

	int GetDescriptor() override
	{
		return _fd;
	}

	bool RequestRead() override;
	bool RequestWrite() override;

	void ProcessRead() override;
	void ProcessWrite() override;

	void ProcessTimeEvent() override;

private:
	EventDispatcher *_dispatcher;
	FailBan *_failBan;

	enum class State
	{
		WaitingSynSize,
		WaitingSyn,
		WaitingAck,
		SendAllAndExit
	};

	State _state;
	int _fd;
	int32_t _ip;
	ServerHandshakeStorage *_storage;
	const Crypto::X25519::PrivateKeyContainer &_privateKey;
	const Crypto::X25519::PublicKeyContainer &_publicKey;

	void ProcessSynSize(CowBuffer<uint8_t> buffer);

	void ProcessSyn(CowBuffer<uint8_t> buffer);
	bool CheckProtocolVersion(const HandshakeSyn::Data &data);
	bool CheckEncryptionScheme(const HandshakeSyn::Data &data);
	String DecryptUserNameFromSyn(
		const HandshakeSyn::Data &data,
		const CowBuffer<uint8_t> buffer);
	void GenerateEphemeralKeys();
	bool GenerateHandshakeKeys();

	void ProcessAck(CowBuffer<uint8_t> buffer);

	Crypto::X25519::EncryptedStream _inES;
	Crypto::X25519::EncryptedStream _outES;
	User *_user;

	CowBuffer<uint8_t> _synSize;
	CowBuffer<uint8_t> _challenge;
	Crypto::X25519::PrivateKeyContainer _ephemeralPrivateKey;
	Crypto::X25519::PublicKeyContainer _ephemeralPublicKey;
	CowBuffer<uint8_t> _salt1;
	CowBuffer<uint8_t> _salt2;

	uint8_t _inScramblerInit;
	uint8_t _outScramblerInit;

	StreamReader *_reader;
	StreamWriter *_writer;

	void HandshakeLog(String name, String message);
};

class ServerHandshakeStorage
{
public:
	virtual ~ServerHandshakeStorage()
	{ }

	virtual void MarkSessionForRemoval(ServerHandshake *session) = 0;

	virtual bool HasUser(String name) = 0;
	virtual User *GetUser(String name) = 0;
};

#endif
