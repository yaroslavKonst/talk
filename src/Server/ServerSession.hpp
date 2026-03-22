#ifndef _SERVER_SESSION_HPP
#define _SERVER_SESSION_HPP

#include "../Protocol/SessionProtocol.hpp"
#include "../Common/EventDispatcher.hpp"
#include "../Common/StreamReader.hpp"
#include "../Common/StreamWriter.hpp"
#include "../Crypto/Crypto.hpp"

class ServerSessionStorage;

class ServerSession :
	public DescriptorEventProcessor,
	public TimeEventProcessor
{
public:
	ServerSession(
		int fd,
		ServerSessionStorage *storage,
		EventDispatcher *dispatcher,
		EncryptedStream *outES,
		EncryptedStream *inES,
		uint8_t outScramblerInit,
		uint8_t inScramblerInit);
	~ServerSession();

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

	int _fd;
	ServerSessionStorage *_storage;

	EncryptedStream _inES;
	EncryptedStream _outES;

	SessionProtocol *_protocol;

	void ProcessInput(CowBuffer<uint8_t> buffer);
};

class ServerSessionStorage
{
public:
	virtual ~ServerSessionStorage()
	{ }

	virtual void MarkSessionForRemoval(ServerSession *session) = 0;
};

#endif
