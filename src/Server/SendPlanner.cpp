#include "SendPlanner.hpp"

#include "../Protocol/GateParser.hpp"
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

	SetTimestamp(GetMonotonicMillisecondTime());
	SetInterval(10000);

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
	ChannelObjectData data;
	data.MessageID = messageID;

	CowBuffer<uint8_t> object = BuildChannelObject(data);

	ObjectStorage::ID newObjectId = _objectStorage.GetFreeID(object);
	_objectStorage.WriteObject(newObjectId, object);

	String refBase = header.Source + " " + header.Destination;
	String headRef = "Head " + refBase;
	String tailRef = "Tail " + refBase;

	if (!_objectStorage.HasRef(tailRef)) {
		_objectStorage.SetRef(headRef, newObjectId);
		_objectStorage.SetRef(tailRef, newObjectId);

		OutboundChannelTreeEntry e;
		e.Source = header.Source;
		e.Destination = header.Destination;

		Tree<OutboundChannelTreeEntry>::Entry *entry =
			_outboundChannels.FindEntry(e);

		if (!entry) {
			e.Planner = this;
			e.Stat = OutboundChannelTreeEntry::Status::Init;

			_outboundChannels.AddEntry(e);
			entry = _outboundChannels.FindEntry(e);
		}

		entry->Key.DeliveryStartTime = GetUnixTime();
		entry->Key.Stat = OutboundChannelTreeEntry::Status::Init;

		ProcessChannel(entry->Key);
	} else {
		ObjectStorage::ID prevTailId = _objectStorage.GetRef(tailRef);

		_objectStorage.UpdateObject(
			prevTailId,
			newObjectId.GetValue(),
			0);

		_objectStorage.SetRef(tailRef, newObjectId);
	}
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

	bool removeEntry = _traverseChannelEntry->Key.Stat ==
		OutboundChannelTreeEntry::Status::Remove;

	Tree<OutboundChannelTreeEntry>::Entry *tmp = _traverseChannelEntry;
	_traverseChannelEntry = _outboundChannels.Next(_traverseChannelEntry);

	if (!removeEntry) {
		ProcessChannel(tmp->Key);
	} else {
		_outboundChannels.RemoveEntry(tmp);
	}

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

bool SendPlanner::OutboundChannelTreeEntry::ReportDeliverySuccess()
{
	bool continueWork = Planner->ReportChannelActionStatus(
		Source,
		Destination,
		true,
		0);

	DeliveryStartTime = GetUnixTime();

	if (!continueWork) {
		Stat = Status::Remove;
	}

	return continueWork;
}

bool SendPlanner::OutboundChannelTreeEntry::ReportDeliveryFailure(
	int32_t reason)
{
	bool continueWork = Planner->ReportChannelActionStatus(
		Source,
		Destination,
		false,
		reason);

	DeliveryStartTime = GetUnixTime();

	if (!continueWork) {
		Stat = Status::Remove;
	}

	return continueWork;
}

CowBuffer<uint8_t> SendPlanner::OutboundChannelTreeEntry::GetMessageForChannel(
	String source,
	String destination)
{
	return Planner->GetMessageForChannel(source, destination);
}

bool SendPlanner::ReportChannelActionStatus(
	String source,
	String destination,
	bool success,
	int32_t errorCode)
{
	String refBase = source + " " + destination;
	String headRef = "Head " + refBase;
	String tailRef = "Tail " + refBase;

	ObjectStorage::ID headId = _objectStorage.GetRef(headRef);

	CowBuffer<uint8_t> object = _objectStorage.ReadObject(headId);

	ChannelObjectData data;
	bool parseResult = ParseChannelObject(object, data);

	if (!parseResult) {
		THROW("Server database contains corrupt outbound queue entry.");
	}

	String userName;
	String hostName;

	Message::SplitFullUserName(source, userName, hostName);

	User *user = _users->GetUser(userName);

	if (user) {
		user->UpdateMessage(
			destination,
			data.MessageID,
			Message::Attribute::InProgress,
			false);
	}

	if (user && !success) {
		Message::Attribute failReason;

		switch (errorCode) {
		case GATE_MESSAGE_HEADER_REJECT:
			failReason = Message::Attribute::Rejected;
			break;
		case GATE_MESSAGE_HEADER_REJECT_INVALID_DESTINATION_USER:
			failReason = Message::Attribute::WrongDestinationUser;
			break;
		case GATE_MESSAGE_HEADER_REJECT_INVALID_DESTINATION_KEY:
			failReason = Message::Attribute::WrongDestinationKey;
			break;
		case GATE_MESSAGE_HEADER_REJECT_INVALID_HEADER:
			failReason = Message::Attribute::InvalidHeader;
			break;
		case GATE_MESSAGE_HEADER_REJECT_MESSAGE_TOO_BIG:
			failReason = Message::Attribute::MessageTooBig;
			break;
		case GATE_MESSAGE_HEADER_REJECT_SENDER_BANNED:
			failReason = Message::Attribute::BannedSender;
			break;
		case GATE_MESSAGE_HEADER_REJECT_SENDER_KEY_BANNED:
			failReason = Message::Attribute::BannedSenderKey;
			break;
		case GATE_MESSAGE_HEADER_REJECT_EXISTS:
			failReason = Message::Attribute::Duplicate;
			break;
		case GATE_MESSAGE_HEADER_REJECT_CONNECTION_FAILURE:
			failReason = Message::Attribute::ConnectionFailure;
			break;
		default:
			failReason = Message::Attribute::Rejected;
			break;
		}

		user->UpdateMessage(
			destination,
			data.MessageID,
			failReason,
			true);
	}

	if (data.NextObject.IsZero()) {
		_objectStorage.DelRef(headRef);
		_objectStorage.DelRef(tailRef);
		_objectStorage.DeleteObject(headId);
		return false;
	}

	_objectStorage.SetRef(headRef, data.NextObject);
	_objectStorage.DeleteObject(headId);
	return true;
}

CowBuffer<uint8_t> SendPlanner::GetMessageForChannel(
	String source,
	String destination)
{
	String refBase = source + " " + destination;
	String headRef = "Head " + refBase;

	ObjectStorage::ID headId = _objectStorage.GetRef(headRef);

	CowBuffer<uint8_t> object = _objectStorage.ReadObject(headId);

	ChannelObjectData data;
	bool parseResult = ParseChannelObject(object, data);

	if (!parseResult) {
		THROW("Server database contains corrupt outbound queue entry.");
	}

	String userName;
	String hostName;

	Message::SplitFullUserName(source, userName, hostName);

	User *user = _users->GetUser(userName);

	if (user) {
		Message::Attribute attr;
		return user->GetMessage(
			destination,
			data.MessageID,
			attr);
	}

	return CowBuffer<uint8_t>();
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

	if (entry.ActionTime > entry.DeliveryStartTime + _maxDeliveryTime) {
		bool continueWork = ReportChannelActionStatus(
			entry.Source,
			entry.Destination,
			false,
			GATE_MESSAGE_HEADER_REJECT_CONNECTION_FAILURE);

		entry.DeliveryStartTime = GetUnixTime();

		if (!continueWork) {
			entry.Stat = OutboundChannelTreeEntry::Status::Remove;
		} else {
			entry.Stat = OutboundChannelTreeEntry::Status::Init;
		}

		return;
	}

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
		e.Planner = this;
		e.DeliveryStartTime = GetUnixTime();
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

bool SendPlanner::ParseChannelObject(
	const CowBuffer<uint8_t> object,
	ChannelObjectData &data)
{
	if (object.Size() != (int)ObjectStorage::Constants::IDSize * 2) {
		return false;
	}

	data.NextObject = object.Pointer();
	data.MessageID = object.Pointer((int)ObjectStorage::Constants::IDSize);
	return true;
}

CowBuffer<uint8_t> SendPlanner::BuildChannelObject(
	const ChannelObjectData &data)
{
	CowBuffer<uint8_t> object((int)ObjectStorage::Constants::IDSize * 2);

	data.NextObject.GetValue(object.Pointer());
	uint64_t offset = (int)ObjectStorage::Constants::IDSize;
	data.MessageID.GetValue(object.Pointer(offset));
	return object;
}
