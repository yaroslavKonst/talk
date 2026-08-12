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

bool RateLimiter::IsAllowed(IPAddress ip)
{
	Tree<RequesterEntry>::Entry *entry = _requests.FindEntry(ip);

	if (!entry) {
		return true;
	}

	ProcessTimeFlowInEntry(entry->Key);

	if (entry->Key.Requests < 1000) {
		return true;
	}

	return false;
}

void RateLimiter::RecordRequest(IPAddress ip)
{
	Tree<RequesterEntry>::Entry *entry = _requests.FindEntry(ip);

	if (!entry) {
		_requests.AddEntry(ip);
		entry = _requests.FindEntry(ip);
	}

	entry->Key.Requests += 1000;
}

void RateLimiter::RecordSessionTimeout(IPAddress ip)
{
	Tree<RequesterEntry>::Entry *entry = _requests.FindEntry(ip);

	if (!entry) {
		_requests.AddEntry(ip);
		entry = _requests.FindEntry(ip);
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
	PreviousActionTimestamp = GetMonotonicMillisecondTime();
	Requests = 0;
}

RateLimiter::RequesterEntry::RequesterEntry(IPAddress ip)
{
	IP = ip;
	PreviousActionTimestamp = GetMonotonicMillisecondTime();
	Requests = 0;
}

bool RateLimiter::RequesterEntry::operator==(const RequesterEntry &e) const
{
	return IP == e.IP;
}

bool RateLimiter::RequesterEntry::operator<(const RequesterEntry &e) const
{
	return IP < e.IP;
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
