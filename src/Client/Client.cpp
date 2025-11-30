#include "Client.hpp"

#include <poll.h>
#include <sys/stat.h>
#include <errno.h>

#include "../Common/Exception.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/SignalHandling.hpp"

Client::Client() : _ui(&_session)
{
	umask(077);

	_session.State = ClientSession::ClientStateUnconnected;
	_session.Processor = nullptr;
}

Client::~Client()
{
}

int Client::Run()
{
	DisableSigPipe();

	bool work = true;

	struct pollfd *fds = new struct pollfd[3];

	fds[0].fd = 0;
	fds[0].events = POLLIN;

	fds[1].fd = _ui.GetSoundReadFileDescriptor();
	fds[1].events = POLLIN;

	int64_t currentTime = GetUnixTime();

	while (work) {
		bool connected = _session.Connected();

		if (connected)
		{
			fds[2].fd = _session.Socket;

			if (_session.CanWrite()) {
				fds[2].events = POLLIN | POLLOUT;
			} else {
				fds[2].events = POLLIN;
			}
		}

		int res = poll(fds, connected ? 3 : 2, 1000);

		if (res == -1) {
			if (errno == EINTR) {
				_ui.ProcessResize();
				work = _ui.ProcessEvent();
				continue;
			} else {
				THROW("Error on poll.");
			}
		}

		int64_t newTime = GetUnixTime();
		bool updateTime = newTime - currentTime >= 2;

		if (updateTime) {
			currentTime = newTime;
		}

		if (fds[0].revents & POLLIN) {
			work = _ui.ProcessEvent();
		}

		if (fds[1].revents & POLLIN) {
			_ui.ProcessSound();
		}

		if (connected) {
			bool endSession = false;

			if (fds[2].revents & POLLOUT) {
				endSession = !_session.Write();
			}

			if (!endSession && (fds[2].revents & POLLIN)) {
				endSession = !_session.Read();
			}

			if (!endSession && _session.CanReceive()) {
				endSession = !_session.Process();
			}

			if (!endSession && updateTime) {
				endSession = !_session.TimePassed();
			}

			if (endSession) {
				_ui.Disconnect();
			}
		}
	}

	delete[] fds;

	return 0;
}
