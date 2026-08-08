#include "ShutdownSignalController.hpp"

#include "../Common/Log.hpp"

ShutdownSignalController::ShutdownSignalController(EventDispatcher *dispatcher)
{
	_dispatcher = dispatcher;

	_dispatcher->RegisterSignalProcessor(this, SIGINT);
	_dispatcher->RegisterSignalProcessor(this, SIGTERM);
	_dispatcher->RegisterSignalProcessor(this, SIGQUIT);
}

ShutdownSignalController::~ShutdownSignalController()
{
	_dispatcher->UnregisterSignalProcessor(this, SIGQUIT);
	_dispatcher->UnregisterSignalProcessor(this, SIGTERM);
	_dispatcher->UnregisterSignalProcessor(this, SIGINT);
}

void ShutdownSignalController::ProcessSignal(int signum)
{
	String signalName;

	if (signum == SIGINT) {
		signalName = "SIGINT";
	} else if (signum == SIGTERM) {
		signalName = "SIGTERM";
	} else if (signum == SIGQUIT) {
		signalName = "SIGQUIT";
	} else {
		return;
	}

	ControllerLog(signalName + " signal is received, requesting shutdown.");
	_dispatcher->Stop();
}

void ShutdownSignalController::ControllerLog(String message)
{
	Log(LogLevel::Warning, "Signal controller", message);
}
