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
		const uint8_t *privateKey,
		const uint8_t *publicKey,
		const uint8_t *serverPublicKey);
	~ClientHandshake();

	bool RequestRead();
	bool RequestWrite();
	bool ProcessRead();
	bool ProcessWrite();

	bool ConnectionSuccessful();

private:
	enum class State
	{
		WaitingSynAck,
		Ready
	};

	State _state;
	int _fd;
	const uint8_t *_privateKey;
	const uint8_t *_publicKey;
	uint8_t _serverPublicKey[KEY_SIZE];

	StreamReader *_reader;
	StreamWriter *_writer;

	EncryptedStream _outES;
	EncryptedStream _inES;

	uint8_t _inScramblerInit;
	uint8_t _outScramblerInit;

	void InitSyn();
	bool ProcessSynAck(CowBuffer<uint8_t> buffer);
};

#endif
