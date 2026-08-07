#include "SendPlanner.hpp"

#include "../Protocol/GateParser.hpp"
#include "../Common/File.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/Log.hpp"

OutboundChannel::OutboundChannel(
	String source,
	String destination,
	EventDispatcher *dispatcher,
	UserDB *users,
	Config *config,
	ObjectStorage *objectStorage,
	SendPlanner *planner)
{
	SetTimestamp(0);
	SetInterval(0);

	_source = source;
	_destination = destination;

	_dispatcher = dispatcher;
	_users = users;
	_config = config;
	_objectStorage = objectStorage;
	_planner = planner;

	_session = nullptr;
	_removeSession = false;

	_deliveryStartTime = GetUnixTime();

	_requestedQuant = false;
	_rateLimitTimeout = false;

	_config->RegisterConfigUser(this);

	StartTransmission();
}

OutboundChannel::~OutboundChannel()
{
	_dispatcher->UnregisterTimeProcessor(this);
	_dispatcher->UnregisterQuantProcessor(this);
	_config->UnregisterConfigUser(this);

	if (_session) {
		delete _session;
		_session = nullptr;
	}

	_dispatcher->UnregisterTimeProcessor(this);
}

void OutboundChannel::ReloadConfig()
{
	if (!GetInterval()) {
		return;
	}

	int64_t interval;

	if (_rateLimitTimeout) {
		interval = _config->GetSendPlannerRequestLimitDelay();
	} else {
		interval = _config->GetSendPlannerConnectionFailureDelay();
	}

	_dispatcher->UnregisterTimeProcessor(this);
	SetInterval(interval * 1000);
	_dispatcher->RegisterTimeProcessor(this);
}

void OutboundChannel::MarkSessionForRemoval(
	OutboundGateSession *session)
{
	_removeSession = true;

	if (!_requestedQuant) {
		_dispatcher->RegisterQuantProcessor(this);
		_requestedQuant = true;
	}
}

void OutboundChannel::ProcessTimeEvent()
{
	_dispatcher->UnregisterTimeProcessor(this);

	SetTimestamp(0);
	SetInterval(0);

	StartTransmission();
}

void OutboundChannel::ProcessQuant()
{
	_requestedQuant = false;

	if (_removeSession) {
		delete _session;
		_session = nullptr;
		_removeSession = false;
	}
}

void OutboundChannel::RegisterMessageForDelivery(
	const Message::X25519::HeaderPointToPoint &header,
	const ObjectStorage::ID &messageID)
{
	ChannelObjectData data;
	data.MessageID = messageID;

	CowBuffer<uint8_t> object = BuildChannelObject(data);

	ObjectStorage::ID newObjectId = _objectStorage->GetFreeID(object);
	_objectStorage->WriteObject(newObjectId, object);

	String refBase = _source + " " + _destination;
	String headRef = "Head " + refBase;
	String tailRef = "Tail " + refBase;

	if (!_objectStorage->HasRef(tailRef)) {
		_objectStorage->SetRef(headRef, newObjectId);
		_objectStorage->SetRef(tailRef, newObjectId);

		_deliveryStartTime = GetUnixTime();

		if (!_session || _removeSession) {
			if (_session) {
				delete _session;
				_session = nullptr;
			}

			_removeSession = false;
			_dispatcher->UnregisterQuantProcessor(this);
			_requestedQuant = false;

			StartTransmission();
		}
	} else {
		ObjectStorage::ID prevTailId =
			_objectStorage->GetRef(tailRef);

		_objectStorage->UpdateObject(
			prevTailId,
			newObjectId.GetValue(),
			0);

		_objectStorage->SetRef(tailRef, newObjectId);
	}
}

void OutboundChannel::ReportConnectionFailure()
{
	_rateLimitTimeout = false;

	SetTimestamp(GetMonotonicMillisecondTime());
	SetInterval(_config->GetSendPlannerConnectionFailureDelay() * 1000);

	_dispatcher->RegisterTimeProcessor(this);
}

void OutboundChannel::ReportRequestRateLimit()
{
	_rateLimitTimeout = true;

	SetTimestamp(GetMonotonicMillisecondTime());
	SetInterval(_config->GetSendPlannerRequestLimitDelay() * 1000);

	_dispatcher->RegisterTimeProcessor(this);
}

bool OutboundChannel::ReportDeliveryStatus(
	bool success,
	int32_t errorCode)
{
	String refBase = _source + " " + _destination;
	String headRef = "Head " + refBase;
	String tailRef = "Tail " + refBase;

	ObjectStorage::ID headId = _objectStorage->GetRef(headRef);

	CowBuffer<uint8_t> object = _objectStorage->ReadObject(headId);

	ChannelObjectData data;
	bool parseResult = ParseChannelObject(object, data);

	if (!parseResult) {
		THROW("Server database contains corrupt outbound queue entry.");
	}

	String userName;
	String hostName;

	Message::SplitFullUserName(_source, userName, hostName);

	User *user = _users->GetUser(userName);

	if (user) {
		user->UpdateMessage(
			_destination,
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
			_destination,
			data.MessageID,
			failReason,
			true);
	}

	_deliveryStartTime = GetUnixTime();

	if (data.NextObject.IsZero()) {
		_objectStorage->DelRef(headRef);
		_objectStorage->DelRef(tailRef);
		_objectStorage->DeleteObject(headId);

		_planner->MarkChannelForRemoval(_source, _destination);
		return false;
	}

	_objectStorage->SetRef(headRef, data.NextObject);
	_objectStorage->DeleteObject(headId);
	return true;
}

CowBuffer<uint8_t> OutboundChannel::GetMessageForChannel()
{
	String refBase = _source + " " + _destination;
	String headRef = "Head " + refBase;

	ObjectStorage::ID headId = _objectStorage->GetRef(headRef);

	CowBuffer<uint8_t> object = _objectStorage->ReadObject(headId);

	ChannelObjectData data;
	bool parseResult = ParseChannelObject(object, data);

	if (!parseResult) {
		THROW("Server database contains corrupt outbound queue entry.");
	}

	String userName;
	String hostName;

	Message::SplitFullUserName(_source, userName, hostName);

	User *user = _users->GetUser(userName);

	if (user) {
		Message::Attribute attr;
		CowBuffer<uint8_t> message = user->GetMessage(
			_destination,
			data.MessageID,
			attr);

		if (message.Size()) {
			return message;
		}
	}

	return CowBuffer<uint8_t>();
}

void OutboundChannel::StartTransmission()
{
	if (_session) {
		return;
	}

	bool stopTrying =
		GetUnixTime() >
		_deliveryStartTime + _config->GetSendPlannerMaxDeliveryTime();

	if (stopTrying) {
		bool continueWork = ReportDeliveryStatus(
			false,
			GATE_MESSAGE_HEADER_REJECT_CONNECTION_FAILURE);

		if (!continueWork) {
			return;
		}
	}

	TaskProcessChannel *task = new TaskProcessChannel;
	task->ReportTarget = this;

	task->Source = _source;
	task->Destination = _destination;

	_removeSession = false;
	_session = new OutboundGateSession(
		_dispatcher,
		_config,
		this,
		task);
}

bool OutboundChannel::ParseChannelObject(
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

CowBuffer<uint8_t> OutboundChannel::BuildChannelObject(
	const ChannelObjectData &data)
{
	CowBuffer<uint8_t> object((int)ObjectStorage::Constants::IDSize * 2);

	data.NextObject.GetValue(object.Pointer());
	uint64_t offset = (int)ObjectStorage::Constants::IDSize;
	data.MessageID.GetValue(object.Pointer(offset));
	return object;
}

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

	_channelsToRemove = nullptr;

	_users->SetSendPlanner(this);

	LoadChannels();
}

SendPlanner::~SendPlanner()
{
	_dispatcher->UnregisterQuantProcessor(this);

	_users->SetSendPlanner(nullptr);

	while (_sessions) {
		SessionNode *tmp = _sessions;
		_sessions = tmp->Next;
		delete tmp->Session;
		delete tmp;
	}

	Tree<OutboundChannelTreeEntry>::Entry *entry =
		_outboundChannels.FindSmallest();

	while (entry) {
		delete entry->Key.Channel;

		Tree<OutboundChannelTreeEntry>::Entry *tmp = entry;
		entry = _outboundChannels.Next(entry);
		_outboundChannels.RemoveEntry(tmp);
	}

	while (_channelsToRemove) {
		ChannelToRemove *tmp = _channelsToRemove;
		_channelsToRemove = _channelsToRemove->Next;
		delete tmp;
	}
}

void SendPlanner::RegisterMessageForDelivery(
	const Message::X25519::HeaderPointToPoint &header,
	const ObjectStorage::ID &messageID)
{
	OutboundChannelTreeEntry e;
	e.Source = header.Source;
	e.Destination = header.Destination;

	Tree<OutboundChannelTreeEntry>::Entry *entry =
		_outboundChannels.FindEntry(e);

	if (!entry) {
		_outboundChannels.AddEntry(e);
		entry = _outboundChannels.FindEntry(e);
		entry->Key.Channel = new OutboundChannel(
			header.Source,
			header.Destination,
			_dispatcher,
			_users,
			_config,
			&_objectStorage,
			this);
	}

	entry->Key.Channel->RegisterMessageForDelivery(header, messageID);

	ChannelToRemove **rmch = &_channelsToRemove;

	while (*rmch) {
		bool removeEntry =
			(*rmch)->Source == header.Source &&
			(*rmch)->Destination == header.Destination;

		if (removeEntry) {
			ChannelToRemove *tmp = *rmch;
			*rmch = (*rmch)->Next;
			delete tmp;
		} else {
			rmch = &(*rmch)->Next;
		}
	}
}

void SendPlanner::ProcessQuant()
{
	if (_hasSessionsForRemoval) {
		RemoveSessions();
	}

	while (_channelsToRemove) {
		OutboundChannelTreeEntry e;
		e.Source = _channelsToRemove->Source;
		e.Destination = _channelsToRemove->Destination;

		Tree<OutboundChannelTreeEntry>::Entry *entry =
			_outboundChannels.FindEntry(e);

		if (entry) {
			delete entry->Key.Channel;
			_outboundChannels.RemoveEntry(entry);
		}

		ChannelToRemove *tmp = _channelsToRemove;
		_channelsToRemove = _channelsToRemove->Next;
		delete tmp;
	}
}

void SendPlanner::MarkSessionForRemoval(OutboundGateSession *session)
{
	SessionNode *sessionNode = _sessions;

	bool needNewQuant = !_hasSessionsForRemoval && !_channelsToRemove;

	while (sessionNode) {
		if (sessionNode->Session == session) {
			sessionNode->Remove = true;
			_hasSessionsForRemoval = true;
		}

		sessionNode = sessionNode->Next;
	}

	if (needNewQuant && _hasSessionsForRemoval) {
		_dispatcher->RegisterQuantProcessor(this);
	}
}

void SendPlanner::MarkChannelForRemoval(String source, String destination)
{
	bool needNewQuant = !_hasSessionsForRemoval && !_channelsToRemove;

	ChannelToRemove *node = new ChannelToRemove;
	node->Next = _channelsToRemove;
	node->Source = source;
	node->Destination = destination;

	_channelsToRemove = node;

	if (needNewQuant) {
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

		_outboundChannels.AddEntry(e);
		Tree<OutboundChannelTreeEntry>::Entry *entry =
			_outboundChannels.FindEntry(e);

		entry->Key.Channel = new OutboundChannel(
			e.Source,
			e.Destination,
			_dispatcher,
			_users,
			_config,
			&_objectStorage,
			this);
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
