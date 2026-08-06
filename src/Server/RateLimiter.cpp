#include "RateLimiter.hpp"

#include "../Common/UnixTime.hpp"

RateLimiter::RateLimiter(EventDispatcher *dispatcher, Config *config)
{
	_dispatcher = dispatcher;
	_config = config;

	_traverseEntry = nullptr;

	ReloadConfig();

	SetTimestamp(GetMonotonicMillisecondTime());
	SetInterval(3600000); // Run cleanup every hour.

	_dispatcher->RegisterTimeProcessor(this);
	_config->RegisterConfigUser(this);
}

RateLimiter::~RateLimiter()
{
	_config->UnregisterConfigUser(this);
	_dispatcher->UnregisterQuantProcessor(this);
	_dispatcher->UnregisterTimeProcessor(this);
}

bool RateLimiter::IsAllowed(uint32_t ipv4)
{
	Tree<RequesterEntry>::Entry *entry = _requests.FindEntry(ipv4);

	if (!entry) {
		return true;
	}

	ProcessTimeFlowInEntry(entry->Key);

	if (entry->Key.Requests < 1000) {
		return true;
	}

	return false;
}

void RateLimiter::RecordRequest(uint32_t ipv4)
{
	Tree<RequesterEntry>::Entry *entry = _requests.FindEntry(ipv4);

	if (!entry) {
		_requests.AddEntry(ipv4);
		entry = _requests.FindEntry(ipv4);
	}

	entry->Key.Requests += 1000;
}

void RateLimiter::RecordSessionTimeout(uint32_t ipv4)
{
	Tree<RequesterEntry>::Entry *entry = _requests.FindEntry(ipv4);

	if (!entry) {
		_requests.AddEntry(ipv4);
		entry = _requests.FindEntry(ipv4);
	}

	entry->Key.Requests +=
		1000 * _sessionTimeoutPenalty * _maxRequestsPerMinute / 60;
}

void RateLimiter::ReloadConfig()
{
	_maxRequestsPerMinute = _config->GetRateLimiterMaxRequestsPerMinute();
	_sessionTimeoutPenalty = _config->GetRateLimiterSessionTimeoutPenalty();
}

void RateLimiter::ProcessQuant()
{
	if (!_traverseEntry) {
		return;
	}

	ProcessTimeFlowInEntry(_traverseEntry->Key);

	Tree<RequesterEntry>::Entry *tmp = _traverseEntry;
	_traverseEntry = _requests.Next(_traverseEntry);

	if (!tmp->Key.Requests) {
		_requests.RemoveEntry(tmp);
	}

	if (_traverseEntry) {
		_dispatcher->RegisterQuantProcessor(this);
	}
}

void RateLimiter::ProcessTimeEvent()
{
	if (_traverseEntry) {
		return;
	}

	_traverseEntry = _requests.FindSmallest();

	if (_traverseEntry) {
		_dispatcher->RegisterQuantProcessor(this);
	}
}

RateLimiter::RequesterEntry::RequesterEntry()
{
	IPv4 = 0;
	PreviousActionTimestamp = GetMonotonicMillisecondTime();
	Requests = 0;
}

RateLimiter::RequesterEntry::RequesterEntry(uint32_t ipv4)
{
	IPv4 = ipv4;
	PreviousActionTimestamp = GetMonotonicMillisecondTime();
	Requests = 0;
}

bool RateLimiter::RequesterEntry::operator==(const RequesterEntry &e) const
{
	return IPv4 == e.IPv4;
}

bool RateLimiter::RequesterEntry::operator<(const RequesterEntry &e) const
{
	return IPv4 < e.IPv4;
}

void RateLimiter::ProcessTimeFlowInEntry(RequesterEntry &entry)
{
	int64_t currentTime = GetMonotonicMillisecondTime();
	int64_t timeDiff = currentTime - entry.PreviousActionTimestamp;

	if (timeDiff < 0) {
		entry.PreviousActionTimestamp = currentTime;
		return;
	}

	if (timeDiff == 0) {
		return;
	}

	uint64_t requestDrop = timeDiff * _maxRequestsPerMinute / 60;

	if (!requestDrop) {
		return;
	}

	if (entry.Requests > requestDrop) {
		entry.Requests -= requestDrop;
	} else {
		entry.Requests = 0;
	}

	entry.PreviousActionTimestamp = currentTime;
}
