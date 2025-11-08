#include "UI.hpp"

#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../Common/Hex.hpp"

static const char *ipMessage = "Enter IP address: ";
static const char *portMessage = "Enter port: ";

static const char *voiceMessage = "Voice status: ";

UI::UI(ClientSession *session)
{
	initscr();
	raw();
	noecho();
	keypad(stdscr, 1);

	_state = UIStatePassword;
	_session = session;

	ProcessResize();
}

UI::~UI()
{
	endwin();
}

void UI::ProcessResize()
{
	getmaxyx(stdscr, _rows, _columns);
	Redraw();
}

void UI::Redraw()
{
	for (int r = 0; r < _rows; r++) {
		for (int c = 0; c < _columns; c++) {
			move(r, c);
			addch(' ');
		}
	}

	switch (_state) {
	case UIStatePassword:
		RedrawPassword();
		break;
	case UIStateConnect:
		RedrawConnect();
		break;
	case UIStateWork:
		RedrawWork();
		break;
	}

	refresh();
}

void UI::RedrawPassword()
{
	move(_rows / 2, 4);
	addstr("Enter password: ");
	addstr(_password.CStr());
}

void UI::RedrawConnect()
{
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

	if (_enterIp) {
		move(
			_rows / 2 - 4,
			4 + strlen(ipMessage) + strlen(_ip.CStr()));
	} else {
		move(
			_rows / 2 - 2,
			4 + strlen(portMessage) + strlen(_port.CStr()));
	}
}

void UI::RedrawWork()
{
	move(0, 0);
	addstr("Help: Exit: END | Mute: Ctrl-M");

	move(2, 0);
	addstr("Chats");

	move(1, _columns / 4);
	addch(ACS_TTEE);

	move(_rows - 2, _columns / 4);
	addch(ACS_BTEE);

	for (int i = 2; i < _rows - 2; i++) {
		move(i, _columns / 4);
		addch(ACS_VLINE);
	}

	for (int i = 0; i < _columns; i++) {
		if (i == _columns / 4) {
			continue;
		}

		move(1, i);
		addch(ACS_HLINE);

		move(_rows - 2, i);
		addch(ACS_HLINE);
	}

	move(_rows - 1, 0);
	addstr(voiceMessage);
	addstr("not connected.");
}

bool UI::ProcessEvent()
{
	int event = getch();

	if (event == KEY_RESIZE) {
		ProcessResize();
		return true;
	}

	if (event == KEY_END) {
		return false;
	}

	switch (_state) {
	case UIStatePassword:
		ProcessPassword(event);
		break;
	case UIStateConnect:
		ProcessConnect(event);
		break;
	case UIStateWork:
		ProcessWork(event);
		break;
	}

	refresh();

	return true;
}

void UI::ProcessPassword(int event)
{
	if (event == KEY_ENTER || event == '\n') {
		if (_password.Length() == 0) {
			int y;
			int x;
			getyx(stdscr, y, x);
			move(_rows / 2 + 2, 10);
			addstr("Password must not be empty.");
			move(y, x);
			return;
		}

		// Transition to connection.
		int y;
		int x;
		getyx(stdscr, y, x);
		move(_rows / 2 + 2, 10);
		addstr("Processing...              ");
		move(y, x);
		refresh();

		uint8_t salt[SALT_SIZE];
		GetSalt("talk.p.salt", salt);
		DeriveKey(_password.CStr(), salt, _session->PrivateKey);
		crypto_wipe(salt, SALT_SIZE);
		GeneratePublicKey(_session->PrivateKey, _session->PublicKey);

		GetSalt("talk.s.salt", salt);
		uint8_t seed[KEY_SIZE];
		DeriveKey(_password.CStr(), salt, seed);
		GenerateSignature(
			seed,
			_session->SignaturePrivateKey,
			_session->SignaturePublicKey);
		crypto_wipe(salt, SALT_SIZE);

		_password.Wipe();

		_state = UIStateConnect;
		_enterIp = true;
		Redraw();
		return;
	}

	if (event == KEY_BACKSPACE || event == '\b') {
		if (_password.Length() == 0) {
			return;
		}

		addch('\b');
		addch(' ');
		addch('\b');

		_password = _password.Substring(0, _password.Length() - 1);
		return;
	}

	if (_password.Length() > 50) {
		int y;
		int x;
		getyx(stdscr, y, x);
		move(_rows / 2 + 2, 10);
		addstr("Password too long.         ");
		move(y, x);
		return;
	}

	_password += event;
	addch(event);
}

void UI::ProcessConnect(int event)
{
	if (event == KEY_ENTER || event == '\n') {
		if (_enterIp) {
			_enterIp = false;
			move(
				_rows / 2 - 2,
				4 + strlen(portMessage) + strlen(_port.CStr()));
			return;
		}

		// Transition to work state.
		{
			int y;
			int x;
			getyx(stdscr, y, x);
			move(_rows / 2 + 2, 10);
			addstr("Connecting...");
			move(y, x);
		}

		uint16_t port = atoi(_port.CStr());

		if (port == 0) {
			int y;
			int x;
			getyx(stdscr, y, x);
			move(_rows / 2 + 2, 10);
			addstr("Invalid port number.");
			move(y, x);
			return;
		}

		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		int res = inet_aton(_ip.CStr(), &addr.sin_addr);

		if (!res) {
			int y;
			int x;
			getyx(stdscr, y, x);
			move(_rows / 2 + 2, 10);
			addstr("Invalid IP address.");
			move(y, x);
			return;
		}

		_session->Socket = socket(AF_INET, SOCK_STREAM, 0);

		if (_session->Socket == -1) {
			int y;
			int x;
			getyx(stdscr, y, x);
			move(_rows / 2 + 2, 10);
			addstr("Failed to create socket.");
			move(y, x);
			return;
		}

		res = connect(
			_session->Socket,
			(struct sockaddr*)&addr,
			sizeof(addr));

		if (res == -1) {
			close(_session->Socket);
			_session->Socket = -1;
			int y;
			int x;
			getyx(stdscr, y, x);
			move(_rows / 2 + 2, 10);
			addstr("Failed to connect.");
			move(y, x);
			return;
		}

		_session->InitSession();

		_state = UIStateWork;
		Redraw();
		return;
	}

	if (event == KEY_BACKSPACE || event == '\b') {
		if (_enterIp) {
			if (_ip.Length() == 0) {
				return;
			}

			_ip = _ip.Substring(0, _ip.Length() - 1);
		} else {
			if (_port.Length() == 0) {
				_enterIp = true;
				move(
					_rows / 2 - 4,
					4 + strlen(ipMessage) +
					strlen(_ip.CStr()));
				return;
			}

			_port = _port.Substring(0, _port.Length() - 1);
		}

		addch('\b');
		addch(' ');
		addch('\b');
		return;
	}

	if (_enterIp) {
		if ((event < '0' || event > '9') && event != '.') {
			return;
		}

		_ip += event;
	} else {
		if (event < '0' || event > '9') {
			return;
		}

		_port += event;
	}

	addch(event);
}

void UI::ProcessWork(int event)
{
}
