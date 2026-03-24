#ifndef _REMOTE_SESSION_HPP
#define _REMOTE_SESSION_HPP

#include "Config.hpp"
#include "../Common/EventDispatcher.hpp"

class GateListeningSocket;

class GateSession
{
public:
	GateSession();
	virtual ~GateSession();

protected:
	int _fd;

	void GateLog(String message);
};

class GateInboundSession :
	public GateSession,
	public DescriptorEventProcessor,
	public TimeEventProcessor
{
public:
	GateInboundSession(
		int fd,
		GateListeningSocket *storage,
		EventDispatcher *dispatcher);
	~GateInboundSession();

	int GetDescriptor() override;
	bool RequestRead() override;
	bool RequestWrite() override;
	void ProcessRead() override;
	void ProcessWrite() override;

	void ProcessTimeEvent() override;

private:
	GateListeningSocket *_storage;
	EventDispatcher *_dispatcher;
};

class OutboundStatusProcessor;

class GateOutboundSession :
	public GateSession,
	public DescriptorEventProcessor,
	public TimeEventProcessor
{
public:
	GateOutboundSession(
		int fd,
		GateListeningSocket *storage,
		EventDispatcher *dispatcher,
		OutboundStatusProcessor *processor);
	~GateOutboundSession();

	int GetDescriptor() override;
	bool RequestRead() override;
	bool RequestWrite() override;
	void ProcessRead() override;
	void ProcessWrite() override;

	void ProcessTimeEvent() override;

private:
	GateListeningSocket *_storage;
	EventDispatcher *_dispatcher;
	OutboundStatusProcessor *_processor;
};

class OutboundStatusProcessor
{
public:
	virtual ~OutboundStatusProcessor()
	{ }

	enum class Status
	{
		SessionEnd
	};

	virtual void ProcessOutboundStatus(
		GateOutboundSession *session,
		Status status) = 0;
};

class GateListeningSocket :
	public DescriptorEventProcessor,
	public QuantEventProcessor,
	public ConfigUser
{
public:
	GateListeningSocket(
		EventDispatcher *dispatcher,
		Config *config);
	~GateListeningSocket();

	void OpenSocket();
	void CloseSocket();

	void ReloadConfig() override;

	int GetDescriptor() override;
	bool RequestRead() override;
	bool RequestWrite() override;
	void ProcessRead() override;
	void ProcessWrite() override;

	void MarkSessionForRemoval(GateSession *session);
	void ProcessQuant() override;

private:
	int _socketFd;
	EventDispatcher *_dispatcher;
	Config *_config;

	struct SessionNode
	{
		SessionNode *Next;
		GateSession *Session;
		bool Remove;
	};

	SessionNode *_sessions;
	bool _timeQuantRequested;
};

#endif
