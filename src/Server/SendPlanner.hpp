#ifndef _SEND_PLANNER_HPP
#define _SEND_PLANNER_HPP

#include "UserDB.hpp"
#include "Config.hpp"
#include "OutboundGateSession.hpp"
#include "../Common/ObjectStorage.hpp"

class SendPlanner;

class OutboundChannel :
	public TaskProcessChannelReportTarget,
	public TimeEventProcessor,
	public QuantEventProcessor,
	public OutboundGateSessionStorage,
	public ConfigUser
{
public:
	OutboundChannel(
		String source,
		String destination,
		EventDispatcher *dispatcher,
		UserDB *users,
		Config *config,
		ObjectStorage *objectStorage,
		SendPlanner *planner);
	~OutboundChannel();

	void ReloadConfig() override;

	void MarkSessionForRemoval(
		OutboundGateSession *session) override;

	void ProcessTimeEvent() override;
	void ProcessQuant() override;

	void RegisterMessageForDelivery(
		const Message::X25519::HeaderPointToPoint &header,
		const ObjectStorage::ID &messageID);

	void ReportConnectionFailure() override;
	void ReportRequestRateLimit() override;

	bool ReportDeliveryStatus(
		bool success,
		int32_t errorCode) override;

	CowBuffer<uint8_t> GetMessageForChannel() override;

private:
	EventDispatcher *_dispatcher;
	UserDB *_users;
	Config *_config;
	ObjectStorage *_objectStorage;
	SendPlanner *_planner;

	String _source;
	String _destination;

	int64_t _deliveryStartTime;

	OutboundGateSession *_session;
	bool _removeSession;
	bool _requestedQuant;

	void StartTransmission();

	bool _rateLimitTimeout;

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

class SendPlanner :
	public SendPlannerBase,
	public OutboundGateSessionStorage,
	public QuantEventProcessor
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

	void ProcessQuant() override;

	void MarkSessionForRemoval(OutboundGateSession *session) override;
	void MarkChannelForRemoval(String source, String destination);

private:
	EventDispatcher *_dispatcher;
	Config *_config;
	UserDB *_users;

	ObjectStorage _objectStorage;

	struct OutboundChannelTreeEntry
	{
		String Source;
		String Destination;

		OutboundChannel *Channel;

		bool operator==(const OutboundChannelTreeEntry &e) const;
		bool operator<(const OutboundChannelTreeEntry &e) const;
	};

	Tree<OutboundChannelTreeEntry> _outboundChannels;

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

	struct ChannelToRemove
	{
		ChannelToRemove *Next;

		String Source;
		String Destination;
	};

	ChannelToRemove *_channelsToRemove;

	bool CanBeDeliveredByShortcut(
		const Message::X25519::HeaderPointToPoint &header);
	void RunDeliveryShortcut(
		const Message::X25519::HeaderPointToPoint &header,
		const ObjectStorage::ID &messageID);
};

#endif
