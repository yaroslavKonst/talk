#ifndef _SEND_PLANNER_HPP
#define _SEND_PLANNER_HPP

#include "UserDB.hpp"
#include "Config.hpp"
#include "OutboundGateSession.hpp"
#include "../Common/ObjectStorage.hpp"

class SendPlanner :
	public SendPlannerBase,
	public OutboundGateSessionStorage,
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

	void MarkSessionForRemoval(OutboundGateSession *session) override;

private:
	EventDispatcher *_dispatcher;
	Config *_config;
	UserDB *_users;

	int64_t _requestLimitTimeSkip;
	int64_t _connectionFailureTimeSkip;
	int64_t _maxDeliveryTime;

	ObjectStorage _objectStorage;

	struct OutboundChannelTreeEntry : public TaskProcessChannelReportTarget
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

		void ReportConnectionFailure() override;
		void ReportRequestRateLimit() override;
	};

	Tree<OutboundChannelTreeEntry> _outboundChannels;
	Tree<OutboundChannelTreeEntry>::Entry *_traverseChannelEntry;

	void ProcessChannel(OutboundChannelTreeEntry &entry);
	void StartTransmission(OutboundChannelTreeEntry &entry);

	void LoadChannels();

	struct SessionNode
	{
		SessionNode *Next;
		OutboundGateSession *Session;
		bool Remove;
	};

	SessionNode *_sessions;
	bool _hasSessionsForRemoval;
	void RemoveSessions();
};

#endif
