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

		SendPlanner *Planner;

		enum class Status
		{
			Init,
			Remove,
			HasActiveSession,
			ReportedRequestLimitOverflow,
			ReportedConnectionFailure
		};

		Status Stat;
		int64_t ActionTime;
		int64_t DeliveryStartTime;

		bool operator==(const OutboundChannelTreeEntry &e) const;
		bool operator<(const OutboundChannelTreeEntry &e) const;

		void ReportConnectionFailure() override;
		void ReportRequestRateLimit() override;

		bool ReportDeliverySuccess();
		bool ReportDeliveryFailure(int32_t reason);
	};

	Tree<OutboundChannelTreeEntry> _outboundChannels;
	Tree<OutboundChannelTreeEntry>::Entry *_traverseChannelEntry;

	bool ReportChannelActionStatus(
		String source,
		String destination,
		bool success,
		int32_t errorCode);

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

	struct ChannelObjectData
	{
		ObjectStorage::ID NextObject;
		ObjectStorage::ID MessageID;
	};

	bool ParseChannelObject(
		const CowBuffer<uint8_t> object,
		ChannelObjectData &data);
	CowBuffer<uint8_t> BuildChannelObject(const ChannelObjectData &data);
};

#endif
