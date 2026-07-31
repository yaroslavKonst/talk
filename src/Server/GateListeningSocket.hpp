#ifndef _GATE_LISTENING_SOCKET_HPP
#define _GATE_LISTENING_SOCKET_HPP

#include "Config.hpp"
#include "InboundGateSession.hpp"
#include "../Common/EventDispatcher.hpp"

class GateListeningSocket :
	public DescriptorEventProcessor,
	public QuantEventProcessor,
	public ConfigUser,
	public InboundGateSessionStorage
{
public:
	GateListeningSocket(
		EventDispatcher *dispatcher,
		Config *config,
		RateLimiter *rateLimiter);
	~GateListeningSocket();

	void OpenSocket();
	void CloseSocket();

	void ReloadConfig() override;

	int GetDescriptor() override;
	bool RequestRead() override;
	bool RequestWrite() override;
	void ProcessRead() override;
	void ProcessWrite() override;

	void MarkSessionForRemoval(InboundGateSession *session) override;
	void ProcessQuant() override;

private:
	int _socketFd;
	EventDispatcher *_dispatcher;
	Config *_config;
	RateLimiter *_rateLimiter;

	struct SessionNode
	{
		SessionNode *Next;
		InboundGateSession *Session;
		bool Remove;
	};

	SessionNode *_sessions;
	bool _timeQuantRequested;
};

#endif
