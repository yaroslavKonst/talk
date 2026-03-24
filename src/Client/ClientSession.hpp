#ifndef _CLIENT_SESSION_HPP
#define _CLIENT_SESSION_HPP

#include "../Protocol/SessionProtocol.hpp"
#include "../Crypto/Crypto.hpp"

class ClientSession
{
public:
	ClientSession(
		int _fd,
		EncryptedStream &outES,
		EncryptedStream &inES,
		uint8_t outScramblerInit,
		uint8_t inScramblerInit);
	~ClientSession();

	bool RequestRead();
	bool RequestWrite();
	bool ProcessRead();
	bool ProcessWrite();

	bool InitKeepAlive();

private:
	int _fd;
	EncryptedStream _outES;
	EncryptedStream _inES;

	SessionProtocol *_protocol;

	int64_t _keepAliveTimestamp;

	bool ProcessInput(const CowBuffer<uint8_t> buffer);

	bool ProcessKeepAlive(const CowBuffer<uint8_t> buffer);

	void SendKeepAlive();
};

#endif
