#include "SendPlanner.hpp"

#include "../Common/File.hpp"
#include "../Common/UnixTime.hpp"

SendPlanner::SendPlanner(
	EventDispatcher *dispatcher,
	Config *config,
	UserDB *users) :
	_objectStorage("storage/SendPlanner", dispatcher)
{
	_dispatcher = dispatcher;
	_config = config;
	_users = users;

	_sessions = nullptr;
	_hasSessionsForRemoval = false;

	_traverseChannelEntry = nullptr;

	_users->SetSendPlanner(this);

	SetTimestamp(GetUnixTime());
	SetInterval(10);

	ReloadConfig();
	LoadChannels();
	ProcessTimeEvent();

	_dispatcher->RegisterTimeProcessor(this);
	_config->RegisterConfigUser(this);
}

SendPlanner::~SendPlanner()
{
	_config->UnregisterConfigUser(this);
	_dispatcher->UnregisterQuantProcessor(this);
	_dispatcher->UnregisterTimeProcessor(this);

	while (_sessions) {
		SessionNode *tmp = _sessions;
		_sessions = tmp->Next;
		delete tmp->Session;
		delete tmp;
	}

	_users->SetSendPlanner(nullptr);
}

void SendPlanner::RegisterMessageForDelivery(
	const Message::X25519::HeaderPointToPoint &header,
	const ObjectStorage::ID &messageID)
{
	//THROW("Not implemented.");
}

void SendPlanner::ProcessTimeEvent()
{
	if (_traverseChannelEntry) {
		return;
	}

	_traverseChannelEntry = _outboundChannels.FindSmallest();

	if (_traverseChannelEntry) {
		_dispatcher->RegisterQuantProcessor(this);
	}
}

void SendPlanner::ProcessQuant()
{
	if (_hasSessionsForRemoval) {
		RemoveSessions();
	}

	if (!_traverseChannelEntry) {
		return;
	}

	ProcessChannel(_traverseChannelEntry->Key);

	_traverseChannelEntry = _outboundChannels.Next(_traverseChannelEntry);

	if (_traverseChannelEntry) {
		_dispatcher->RegisterQuantProcessor(this);
	}
}

void SendPlanner::ReloadConfig()
{
	_requestLimitTimeSkip = _config->GetSendPlannerRequestLimitDelay();
	_connectionFailureTimeSkip =
		_config->GetSendPlannerConnectionFailureDelay();
	_maxDeliveryTime = _config->GetSendPlannerMaxDeliveryTime();
}

void SendPlanner::MarkSessionForRemoval(OutboundGateSession *session)
{
	SessionNode *sessionNode = _sessions;

	bool needNewQuant = !_hasSessionsForRemoval;

	while (sessionNode) {
		if (sessionNode->Session == session) {
			sessionNode->Remove = true;
			_hasSessionsForRemoval = true;
		}

		sessionNode = sessionNode->Next;
	}

	if (needNewQuant && _hasSessionsForRemoval && !_traverseChannelEntry) {
		_dispatcher->RegisterQuantProcessor(this);
	}
}

bool SendPlanner::OutboundChannelTreeEntry::operator==(
	const OutboundChannelTreeEntry &e) const
{
	return Source == e.Source && Destination == e.Destination;
}

bool SendPlanner::OutboundChannelTreeEntry::operator<(
	const OutboundChannelTreeEntry &e) const
{
	if (Source != e.Source) {
		return Source < e.Source;
	}

	return Destination < e.Destination;
}

void SendPlanner::OutboundChannelTreeEntry::ReportConnectionFailure()
{
	Stat = Status::ReportedConnectionFailure;
	ActionTime = GetUnixTime();
}

void SendPlanner::OutboundChannelTreeEntry::ReportRequestRateLimit()
{
	Stat = Status::ReportedRequestLimitOverflow;
	ActionTime = GetUnixTime();
}

void SendPlanner::ProcessChannel(OutboundChannelTreeEntry &entry)
{
	if (entry.Stat == OutboundChannelTreeEntry::Status::HasActiveSession) {
		return;
	}

	if (entry.Stat == OutboundChannelTreeEntry::Status::Init) {
		StartTransmission(entry);
		return;
	}

	int64_t procTime = entry.ActionTime;

	if (entry.Stat ==
		OutboundChannelTreeEntry::Status::ReportedRequestLimitOverflow)
	{
		procTime += _requestLimitTimeSkip;
	} else if (entry.Stat ==
		OutboundChannelTreeEntry::Status::ReportedConnectionFailure)
	{
		procTime += _connectionFailureTimeSkip;
	}

	if (procTime > GetUnixTime()) {
		return;
	}

	StartTransmission(entry);
}

void SendPlanner::StartTransmission(OutboundChannelTreeEntry &entry)
{
	entry.ActionTime = GetUnixTime();
	entry.Stat = OutboundChannelTreeEntry::Status::HasActiveSession;

	TaskProcessChannel *task = new TaskProcessChannel;
	task->ReportTarget = &entry;

	task->Source = entry.Source;
	task->Destination = entry.Destination;

	SessionNode *node = new SessionNode;
	node->Remove = false;
	node->Session = new OutboundGateSession(
		_dispatcher,
		_config,
		this,
		task);
	node->Next = _sessions;

	_sessions = node;
}

void SendPlanner::LoadChannels()
{
	CowBuffer<String> channelNames =
		ListDirectory("storage/SendPlanner/refs");

	for (unsigned int i = 0; i < channelNames.Size(); i++) {
		CowBuffer<String> parts = channelNames[i].Split(' ', false);

		if (parts.Size() != 3) {
			THROW("Invalid SendPlanner database ref.");
		}

		if (parts[0] == "Tail") {
			continue;
		} else if (parts[0] != "Head") {
			THROW("Invalid SendPlanner database ref.");
		}

		OutboundChannelTreeEntry e;
		e.Source = parts[1];
		e.Destination = parts[2];
		e.Stat = OutboundChannelTreeEntry::Status::Init;

		_outboundChannels.AddEntry(e);
	}
}

void SendPlanner::RemoveSessions()
{
	SessionNode **session = &_sessions;

	while (*session) {
		if ((*session)->Remove) {
			SessionNode *tmp = *session;
			*session = (*session)->Next;
			delete tmp->Session;
			delete tmp;
		} else {
			session = &(*session)->Next;
		}
	}

	_hasSessionsForRemoval = false;
}
