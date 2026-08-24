#ifndef _OUTBOUND_TASK_BASE_HPP
#define _OUTBOUND_TASK_BASE_HPP

#include "../Common/MyString.hpp"
#include "../Common/CowBuffer.hpp"

class OutboundTaskBase
{
public:
	OutboundTaskBase();
	virtual ~OutboundTaskBase()
	{ }

	virtual bool MustBeDeleted()
	{
		return true;
	}

	virtual void NotifyGateSessionClosed()
	{ }

	virtual int64_t GetTimeoutInterval()
	{
		return 60000;
	}

	virtual bool TimeoutAction()
	{
		return false;
	}

	virtual String GetConnectionDestination() = 0;
	virtual void ReportConnectionFailure() = 0;
	virtual void ReportRequestRateLimit() = 0;

	// Has data to write.
	virtual bool HasData() = 0;
	// Get data. Returned empty buffer must close the session.
	virtual CowBuffer<uint8_t> GetData() = 0;
	// Process data. Returned false must close the session.
	virtual bool ProcessData(const CowBuffer<uint8_t> buffer) = 0;

protected:
	bool MustReportFailure();
	void MarkFailureReport();
	void AllowFailureReport();

private:
	bool _reportedFailure;
};

#endif
