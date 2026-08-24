#include "OutboundTaskBase.hpp"

OutboundTaskBase::OutboundTaskBase()
{
	_reportedFailure = false;
}

bool OutboundTaskBase::MustReportFailure()
{
	return !_reportedFailure;
}

void OutboundTaskBase::MarkFailureReport()
{
	_reportedFailure = true;
}

void OutboundTaskBase::AllowFailureReport()
{
	_reportedFailure = false;
}
