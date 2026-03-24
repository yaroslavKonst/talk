#ifndef _SERVER_SESSION_HPP
#define _SERVER_SESSION_HPP

#include "Config.hpp"
#include "../Protocol/SessionProtocol.hpp"
#include "../Common/EventDispatcher.hpp"
#include "../Common/StreamReader.hpp"
#include "../Common/StreamWriter.hpp"
#include "../Crypto/Crypto.hpp"

class ServerSessionStorage;

class ServerSession :
	public DescriptorEventProcessor,
	public TimeEventProcessor,
	public ConfigUser
{
public:
	ServerSession(
		int fd,
		ServerSessionStorage *storage,
		Config* config,
		EventDispatcher *dispatcher,
		EncryptedStream *outES,
		EncryptedStream *inES,
		uint8_t outScramblerInit,
		uint8_t inScramblerInit);
	~ServerSession();

	void ReloadConfig() override;

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
	Config *_config;

	EncryptedStream _inES;
	EncryptedStream _outES;

	SessionProtocol *_protocol;

	bool ProcessInput(const CowBuffer<uint8_t> buffer);

	bool ProcessKeepAlive(const CowBuffer<uint8_t> buffer);

	void SessionLog(String message);
};

class ServerSessionStorage
{
public:
	virtual ~ServerSessionStorage()
	{ }

	virtual void MarkSessionForRemoval(ServerSession *session) = 0;

	virtual String GetName() = 0;
};

#endif
