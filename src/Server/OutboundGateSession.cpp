#include "OutboundGateSession.hpp"

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "../Common/Exception.hpp"
#include "../Common/File.hpp"
#include "../Common/Log.hpp"

GateOutboundSession::GateOutboundSession(
	int fd,
	GateListeningSocket *storage,
	EventDispatcher *dispatcher,
	OutboundStatusProcessor *processor)
{
	SetInterval(60);
	SetTimestamp(GetUnixTime());

	_fd = fd;
	_storage = storage;
	_dispatcher = dispatcher;
	_processor = processor;

	_dispatcher->RegisterTimeProcessor(this);

	_storage->MarkSessionForRemoval(this);
	GateLog("Outbound connection attempt occured.");
}

GateOutboundSession::~GateOutboundSession()
{
	_dispatcher->UnregisterTimeProcessor(this);

	_processor->ProcessOutboundStatus(
		this,
		OutboundStatusProcessor::Status::SessionEnd);
}

int GateOutboundSession::GetDescriptor()
{
	return _fd;
}

bool GateOutboundSession::RequestRead()
{
	THROW("Not implemented.");
}

bool GateOutboundSession::RequestWrite()
{
	THROW("Not implemented.");
}

void GateOutboundSession::ProcessRead()
{
	THROW("Not implemented.");
}

void GateOutboundSession::ProcessWrite()
{
	THROW("Not implemented.");
}

void GateOutboundSession::ProcessTimeEvent()
{
	_storage->MarkSessionForRemoval(this);
}
