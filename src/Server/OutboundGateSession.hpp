#ifndef _OUTBOUND_GATE_SESSION_HPP
#define _OUTBOUND_GATE_SESSION_HPP

#include "Config.hpp"
#include "../Common/EventDispatcher.hpp"

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

#endif
