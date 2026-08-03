#ifndef _SHUTDOWN_SIGNAL_CONTROLLER_HPP
#define _SHUTDOWN_SIGNAL_CONTROLLER_HPP

#include "../Common/EventDispatcher.hpp"

class ShutdownSignalController : public SignalEventProcessor
{
public:
	ShutdownSignalController(EventDispatcher *dispatcher);
	~ShutdownSignalController();

	void ProcessSignal(int signum) override;

private:
	EventDispatcher *_dispatcher;

	void ControllerLog(String message);
};

#endif
