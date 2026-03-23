#ifndef _SERVER_HANDSHAKE_HPP
#define _SERVER_HANDSHAKE_HPP

#include "User.hpp"
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
		ServerHandshakeStorage *storage,
		EventDispatcher *dispatcher,
		const uint8_t *privateKey,
		const uint8_t *publicKey);
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

	enum class State
	{
		WaitingSize,
		WaitingSyn,
		WaitingAck
	};

	State _state;
	int _fd;
	ServerHandshakeStorage *_storage;
	const uint8_t *_privateKey;
	const uint8_t *_publicKey;

	void ProcessSize(CowBuffer<uint8_t> buffer);
	void ProcessSyn(CowBuffer<uint8_t> buffer);
	void ProcessAck(CowBuffer<uint8_t> buffer);

	EncryptedStream _inES;
	EncryptedStream _outES;
	User *_user;
	CowBuffer<uint8_t> _nameSize;

	CowBuffer<uint8_t> _challenge;

	uint8_t _inScramblerInit;
	uint8_t _outScramblerInit;

	StreamReader *_reader;
	StreamWriter *_writer;
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
