#include "SendPlanner.hpp"

SendPlanner::SendPlanner(EventDispatcher *dispatcher, UserDB *users) :
	_objectStorage("storage/SendPlanner", dispatcher)
{
	_dispatcher = dispatcher;
	_users = users;

	_users->SetSendPlanner(this);
}

SendPlanner::~SendPlanner()
{
	_users->SetSendPlanner(nullptr);
}

void SendPlanner::RegisterMessageForDelivery(
	const Message::X25519::HeaderPointToPoint &header,
	const ObjectStorage::ID &messageID)
{
	THROW("Not implemented.");
}
