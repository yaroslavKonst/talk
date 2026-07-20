#ifndef _CLIENT_HANDSHAKE_HPP
#define _CLIENT_HANDSHAKE_HPP

#include "../Common/StreamReader.hpp"
#include "../Common/StreamWriter.hpp"
#include "../Crypto/Crypto.hpp"

class ClientHandshake
{
public:
	ClientHandshake(
		int fd,
		String name,
		const Crypto::X25519::PrivateKeyContainer &privateKey,
		const Crypto::X25519::PublicKeyContainer &publicKey,
		const Crypto::X25519::PublicKeyContainer &serverPublicKey);
	~ClientHandshake();

	bool RequestRead();
	bool RequestWrite();
	bool ProcessRead();
	bool ProcessWrite();

	bool ConnectionSuccessful();

	Crypto::X25519::EncryptedStream &GetOutES()
	{
		return _outES;
	}

	Crypto::X25519::EncryptedStream &GetInES()
	{
		return _inES;
	}

	uint8_t GetOutScramblerInit()
	{
		return _outScramblerInit;
	}

	uint8_t GetInScramblerInit()
	{
		return _inScramblerInit;
	}

private:
	enum class State
	{
		WaitingSynAck,
		Ready
	};

	State _state;
	int _fd;
	String _name;
	const Crypto::X25519::PrivateKeyContainer &_privateKey;
	const Crypto::X25519::PublicKeyContainer &_publicKey;
	Crypto::X25519::PublicKeyContainer _serverPublicKey;

	StreamReader *_reader;
	StreamWriter *_writer;

	Crypto::X25519::EncryptedStream _outES;
	Crypto::X25519::EncryptedStream _inES;

	uint8_t _inScramblerInit;
	uint8_t _outScramblerInit;

	void InitSyn();
	bool ProcessSynAck(CowBuffer<uint8_t> buffer);
};

#endif
