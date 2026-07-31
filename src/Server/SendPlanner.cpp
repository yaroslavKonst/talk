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

	_users->SetSendPlanner(nullptr);
}

void SendPlanner::RegisterMessageForDelivery(
	const Message::X25519::HeaderPointToPoint &header,
	const ObjectStorage::ID &messageID)
{
	THROW("Not implemented.");
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
	if (!_traverseChannelEntry) {
		return;
	}

#warning TODO: entry processing.

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
		e.Stat = OutboundChannelTreeEntry::Status::Init;

		_outboundChannels.AddEntry(e);
	}
}
