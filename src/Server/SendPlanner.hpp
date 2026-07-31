#ifndef _SEND_PLANNER_HPP
#define _SEND_PLANNER_HPP

#include "UserDB.hpp"
#include "Config.hpp"
#include "../Common/ObjectStorage.hpp"

class SendPlanner :
	public SendPlannerBase,
	public TimeEventProcessor,
	public QuantEventProcessor,
	public ConfigUser
{
public:
	SendPlanner(
		EventDispatcher *dispatcher,
		Config *config,
		UserDB *users);
	~SendPlanner();

	void RegisterMessageForDelivery(
		const Message::X25519::HeaderPointToPoint &header,
		const ObjectStorage::ID &messageID) override;

	void ProcessTimeEvent() override;
	void ProcessQuant() override;

	void ReloadConfig() override;

private:
	EventDispatcher *_dispatcher;
	Config *_config;
	UserDB *_users;

	int64_t _requestLimitTimeSkip;
	int64_t _connectionFailureTimeSkip;
	int64_t _maxDeliveryTime;

	ObjectStorage _objectStorage;

	struct OutboundChannelTreeEntry
	{
		String Source;
		String Destination;

		enum class Status
		{
			Init,
			HasActiveSession,
			ReportedRequestLimitOverflow,
			ReportedConnectionFailure
		};

		Status Stat;
		int64_t ActionTime;

		bool operator==(const OutboundChannelTreeEntry &e) const;
		bool operator<(const OutboundChannelTreeEntry &e) const;
	};

	Tree<OutboundChannelTreeEntry> _outboundChannels;
	Tree<OutboundChannelTreeEntry>::Entry *_traverseChannelEntry;

	void LoadChannels();
};

class OutboundGateSessionBase
{
public:
	virtual ~OutboundGateSessionBase()
	{ }
};

#endif
