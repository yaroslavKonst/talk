#ifndef _SEND_PLANNER_HPP
#define _SEND_PLANNER_HPP

#include "UserDB.hpp"
#include "../Common/ObjectStorage.hpp"

class SendPlanner :
	public SendPlannerBase
{
public:
	SendPlanner(EventDispatcher *dispatcher, UserDB *users);
	~SendPlanner();

	void RegisterMessageForDelivery(
		const Message::X25519::HeaderPointToPoint &header,
		const ObjectStorage::ID &messageID) override;

private:
	EventDispatcher *_dispatcher;
	UserDB *_users;

	ObjectStorage _objectStorage;
};

class OutboundGateSessionBase
{
public:
	virtual ~OutboundGateSessionBase()
	{ }
};

#endif
