#include "SignalHandling.hpp"

#include <signal.h>

#include "Exception.hpp"

void DisableSigPipe()
{
	struct sigaction act;
	memset(&act, 0, sizeof(act));
	act.sa_handler = SIG_IGN;

	int res = sigaction(SIGPIPE, &act, nullptr);

	if (res == -1) {
		THROW("Failed to ignore SIGPIPE signal.");
	}
}
