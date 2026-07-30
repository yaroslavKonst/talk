#ifndef _RATE_LIMITER_HPP
#define _RATE_LIMITER_HPP

#include "Config.hpp"
#include "../Common/EventDispatcher.hpp"
#include "../Common/Tree.hpp"

class RateLimiter :
	public QuantEventProcessor,
	public TimeEventProcessor,
	public ConfigUser
{
public:
	RateLimiter(EventDispatcher *dispatcher, Config *config);
	~RateLimiter();

	bool IsAllowed(uint32_t ipv4);
	void RecordRequest(uint32_t ipv4);
	void RecordSessionTimeout(uint32_t ipv4);

	void ReloadConfig() override;

	void ProcessQuant() override;
	void ProcessTimeEvent() override;

private:
	EventDispatcher *_dispatcher;
	Config *_config;

	uint64_t _maxRequestsPerMinute;
	uint64_t _sessionTimeoutPenalty;

	struct RequesterEntry
	{
		uint32_t IPv4;
		int64_t PreviousActionTimestamp; // Measured in milliseconds.
		uint64_t Requests; // Holds request number multiplied by 1000.

		RequesterEntry();
		RequesterEntry(uint32_t ipv4);

		bool operator==(const RequesterEntry &e) const;
		bool operator<(const RequesterEntry &e) const;
	};

	Tree<RequesterEntry> _requests;
	Tree<RequesterEntry>::Entry *_traverseEntry;

	void ProcessTimeFlowInEntry(RequesterEntry &entry);
};

#endif
