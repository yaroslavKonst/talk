#include "LoginScreen.hpp"

#include <unistd.h>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <curses.h>

#include "../Common/Hex.hpp"

static const char *ipMessage = "Enter IP address: ";
static const char *portMessage = "Enter port: ";

LoginScreen::LoginScreen(ClientSession *session, IniFile *configFile) :
	Screen(session)
{
	_writingIp = true;
	_writingPort = false;
	_writingKey = false;

	_ip = configFile->Get("connection", "ServerIP");
	_port = configFile->Get("connection", "ServerPort");
	_serverKeyHex = configFile->Get("connection", "ServerKey");

	if (_ip.Length()) {
		_writingIp = false;
		_writingPort = true;
	}

	if (_writingPort && _port.Length()) {
		_writingPort = false;
		_writingKey = true;
	}
}

void LoginScreen::Redraw()
{
	ClearScreen();

	move(0, 0);
	addstr("Exit: End | Next: Enter/Down | Previous: Up | Connect: Enter");

	move(_rows / 2 - 10, 4);
	addstr("Your public key:");
	move(_rows / 2 - 9, 4);
	String hex = DataToHex(_session->PublicKey, KEY_SIZE);
	addstr(hex.CStr());

	move(_rows / 2 - 7, 4);
	addstr("Your signature:");
	move(_rows / 2 - 6, 4);
	hex = DataToHex(_session->SignaturePublicKey, KEY_SIZE);
	addstr(hex.CStr());

	move(_rows / 2 - 4, 4);
	addstr(ipMessage);
	addstr(_ip.CStr());

	move(_rows / 2 - 2, 4);
	addstr(portMessage);
	addstr(_port.CStr());

	move(_rows / 2, 4);
	addstr("Server public key:");
	move(_rows / 2 + 1, 4);
	addstr(_serverKeyHex.CStr());

	if (_status.Length() > 0) {
		move(_rows / 2 + 3, _columns / 2 - _status.Length() / 2);
		addstr(_status.CStr());
	}

	if (_writingIp) {
		move(
			_rows / 2 - 4,
			4 + strlen(ipMessage) + strlen(_ip.CStr()));
	} else if (_writingPort) {
		move(
			_rows / 2 - 2,
			4 + strlen(portMessage) + strlen(_port.CStr()));
	} else if (_writingKey) {
		move(
			_rows / 2 + 1,
			4 + strlen(_serverKeyHex.CStr()));
	}

	refresh();
}

Screen *LoginScreen::ProcessEvent(int event)
{
	if (event == KEY_END) {
		return nullptr;
	}

	if (event == KEY_UP) {
		if (_writingPort) {
			_writingPort = false;
			_writingIp = true;
		} else if (_writingKey) {
			_writingKey = false;
			_writingPort = true;
		}

		return this;
	}

	if (event == KEY_DOWN) {
		if (_writingIp) {
			_writingIp = false;
			_writingPort = true;
		} else if (_writingPort) {
			_writingPort = false;
			_writingKey = true;
		}

		return this;
	}

	if (event == KEY_ENTER || event == '\n') {
		if (_writingIp) {
			_writingIp = false;
			_writingPort = true;
			return this;
		} else if (_writingPort) {
			_writingPort = false;
			_writingKey = true;
			return this;
		}

		// Transition to work state.
		if (_serverKeyHex.Length() != KEY_SIZE * 2) {
			_status = "Invalid server key length.";
			return this;
		}

		HexToData(_serverKeyHex, _session->PeerPublicKey);

		uint16_t port = atoi(_port.CStr());

		if (port == 0) {
			_status = "Invalid port number.";
			return this;
		}

		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		int res = inet_aton(_ip.CStr(), &addr.sin_addr);

		if (!res) {
			_status = "Invalid IP address.";
			return this;
		}

		_status = "Connecting...";
		Redraw();

		_session->Socket = socket(AF_INET, SOCK_STREAM, 0);

		if (_session->Socket == -1) {
			_status = "Failed to create socket.";
			return this;
		}

		res = connect(
			_session->Socket,
			(struct sockaddr*)&addr,
			sizeof(addr));

		if (res == -1) {
			close(_session->Socket);
			_session->Socket = -1;
			_status = "Failed to connect.";
			return this;
		}

		bool initRes = _session->InitSession();

		if (!initRes) {
			_session->Close();
			_status = "Failed to init session.";
			return this;
		}

		return nullptr;
	}

	if (event == KEY_BACKSPACE || event == '\b') {
		if (_writingIp) {
			if (_ip.Length() == 0) {
				return this;
			}

			_ip = _ip.Substring(0, _ip.Length() - 1);
		} else if (_writingPort) {
			if (_port.Length() == 0) {
				return this;
			}

			_port = _port.Substring(0, _port.Length() - 1);
		} else if (_writingKey) {
			if (_serverKeyHex.Length() == 0) {
				return this;
			}

			_serverKeyHex = _serverKeyHex.Substring(
				0,
				_serverKeyHex.Length() - 1);
		}

		return this;
	}

	if (_writingIp) {
		if ((event < '0' || event > '9') && event != '.') {
			return this;
		}

		_ip += event;
	} else if (_writingPort) {
		if (event < '0' || event > '9') {
			return this;
		}

		_port += event;
	} else if (_writingKey) {
		_serverKeyHex += event;
	}

	return this;
}
