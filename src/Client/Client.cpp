#include "Client.hpp"

#include <poll.h>

Client::Client() : _ui(&_session)
{
	_session.Socket = -1;
	_session.State = ClientSession::ClientStateUnconnected;
}

Client::~Client()
{
}

int Client::Run()
{
	bool work = true;

	struct pollfd *fds = new struct pollfd[2];

	fds[0].fd = 0;
	fds[0].events = POLLIN;

	int64_t currentTime = GetUnixTime();

	while (work) {
		bool connected = _session.Socket != -1;

		if (connected)
		{
			fds[1].fd = _session.Socket;

			if (_session.CanWrite()) {
				fds[1].events = POLLIN | POLLOUT;
			} else {
				fds[1].events = POLLIN;
			}
		}

		int res = poll(fds, connected ? 2 : 1, 1000);

		if (res == -1) {
			work = false;
		}

		int64_t newTime = GetUnixTime();
		bool updateTime = newTime - currentTime >= 2;

		if (updateTime) {
			currentTime = newTime;
		}

		if (fds[0].revents & POLLIN) {
			work = _ui.ProcessEvent();
		}

		if (connected) {
			bool endSession = false;

			if (fds[1].revents & POLLOUT) {
				endSession = !_session.Write();
			}

			if (!endSession && (fds[1].revents & POLLIN)) {
				endSession = !_session.Read();
			}

			if (!endSession && _session.CanReceive()) {
				endSession = !_session.Process();
			}

			if (!endSession && updateTime) {
				endSession = !_session.TimePassed();
			}

			if (endSession) {
				work = false;
			}
		}
	}

	return 0;
}
