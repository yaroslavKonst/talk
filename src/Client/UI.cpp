#include "UI.hpp"

#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../Common/Hex.hpp"

static const char *voiceMessage = "Voice status: ";

// Base screen.
Screen::Screen(ClientSession * session)
{
	_session = session;
	getmaxyx(stdscr, _rows, _columns);
}

Screen::~Screen()
{
}

void Screen::ProcessResize()
{
	getmaxyx(stdscr, _rows, _columns);
	Redraw();
}

void Screen::ClearScreen()
{
	for (int r = 0; r < _rows; r++) {
		for (int c = 0; c < _columns; c++) {
			move(r, c);
			addch(' ');
		}
	}
}

// Password screen.
PasswordScreen::PasswordScreen(ClientSession *session) : Screen(session)
{
}

void PasswordScreen::Redraw()
{
	ClearScreen();

	move(0, 0);
	addstr("Help: Exit: END | Mute: Ctrl-M | Mark read: Ctrl-R");

	move(_rows / 2, 4);
	addstr("Enter password: ");
	addstr(_password.CStr());
	refresh();
}

Screen *PasswordScreen::ProcessEvent(int event)
{
	if (event == KEY_ENTER || event == '\n') {
		if (_password.Length() == 0) {
			int y;
			int x;
			getyx(stdscr, y, x);
			move(_rows / 2 + 2, 10);
			addstr("Password must not be empty.");
			move(y, x);
			return this;
		}

		// Transition to connection.
		int y;
		int x;
		getyx(stdscr, y, x);
		move(_rows / 2 + 2, 10);
		addstr("Processing...              ");
		move(y, x);
		refresh();

		GenerateKeys();

		return new LoginScreen(_session);
	}

	if (event == KEY_BACKSPACE || event == '\b') {
		if (_password.Length() == 0) {
			return this;
		}

		addch('\b');
		addch(' ');
		addch('\b');

		_password = _password.Substring(0, _password.Length() - 1);
		return this;
	}

	if (_password.Length() > 50) {
		int y;
		int x;
		getyx(stdscr, y, x);
		move(_rows / 2 + 2, 10);
		addstr("Password too long.         ");
		move(y, x);
		return this;
	}

	_password += event;
	addch(event);

	return this;
}

void PasswordScreen::GenerateKeys()
{
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
}

// Login screen.
static const char *ipMessage = "Enter IP address: ";
static const char *portMessage = "Enter port: ";

LoginScreen::LoginScreen(ClientSession *session) : Screen(session)
{
	_writingIp = true;
	_writingPort = false;
	_writingKey = false;
}

void LoginScreen::Redraw()
{
	ClearScreen();

	move(0, 0);
	addstr("Help: Exit: END | Mute: Ctrl-M | Mark read: Ctrl-R");

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
	if (event == KEY_ENTER || event == '\n') {
		if (_writingIp) {
			_writingIp = false;
			_writingPort = true;
			move(
				_rows / 2 - 2,
				4 + strlen(portMessage) + strlen(_port.CStr()));
			return this;
		} else if (_writingPort) {
			_writingPort = false;
			_writingKey = true;
			move(
				_rows / 2 + 1,
				4 + strlen(_serverKeyHex.CStr()));
			return this;
		}

		// Transition to work state.
		HexToData(_serverKeyHex, _session->PeerPublicKey);

		{
			int y;
			int x;
			getyx(stdscr, y, x);
			move(_rows / 2 + 3, 10);
			addstr("Connecting...");
			move(y, x);
		}

		uint16_t port = atoi(_port.CStr());

		if (port == 0) {
			int y;
			int x;
			getyx(stdscr, y, x);
			move(_rows / 2 + 3, 10);
			addstr("Invalid port number.");
			move(y, x);
			return this;
		}

		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		int res = inet_aton(_ip.CStr(), &addr.sin_addr);

		if (!res) {
			int y;
			int x;
			getyx(stdscr, y, x);
			move(_rows / 2 + 3, 10);
			addstr("Invalid IP address.");
			move(y, x);
			return this;
		}

		_session->Socket = socket(AF_INET, SOCK_STREAM, 0);

		if (_session->Socket == -1) {
			int y;
			int x;
			getyx(stdscr, y, x);
			move(_rows / 2 + 3, 10);
			addstr("Failed to create socket.");
			move(y, x);
			return this;
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
			move(_rows / 2 + 3, 10);
			addstr("Failed to connect.");
			move(y, x);
			return this;
		}

		bool initRes = _session->InitSession();

		if (!initRes) {
			close(_session->Socket);
			_session->Socket = -1;
			int y;
			int x;
			getyx(stdscr, y, x);
			move(_rows / 2 + 3, 10);
			addstr("Failed to init session.");
			move(y, x);
			return this;
		}

		return new WorkScreen(_session);
	}

	if (event == KEY_BACKSPACE || event == '\b') {
		if (_writingIp) {
			if (_ip.Length() == 0) {
				return this;
			}

			_ip = _ip.Substring(0, _ip.Length() - 1);
		} else if (_writingPort) {
			if (_port.Length() == 0) {
				_writingPort = false;
				_writingIp = true;
				move(
					_rows / 2 - 4,
					4 + strlen(ipMessage) +
					strlen(_ip.CStr()));
				return this;
			}

			_port = _port.Substring(0, _port.Length() - 1);
		} else if (_writingKey) {
			if (_serverKeyHex.Length() == 0) {
				_writingKey = false;
				_writingPort = true;
				move(
					_rows / 2 - 2,
					4 + strlen(portMessage) +
					strlen(_port.CStr()));
				return this;
			}

			_serverKeyHex = _serverKeyHex.Substring(
				0,
				_serverKeyHex.Length() - 1);
		}

		addch('\b');
		addch(' ');
		addch('\b');
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

	addch(event);
	return this;
}

// Work screen.
WorkScreen::WorkScreen(ClientSession *session) : Screen(session)
{
	_chat = nullptr;
}

WorkScreen::~WorkScreen()
{
	if (_chat) {
		delete _chat;
	}
}

void WorkScreen::Redraw()
{
	ClearScreen();

	move(0, 0);
	addstr("Help: Exit: END | Mute: Ctrl-M | Mark read: Ctrl-R");

	move(2, 0);
	addstr("User");

	move(3, 0);
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

	refresh();
}

Screen *WorkScreen::ProcessEvent(int event)
{
	return this;
}

// UI main.
UI::UI(ClientSession *session)
{
	initscr();
	raw();
	noecho();
	keypad(stdscr, 1);

	_session = session;
	_screen = new PasswordScreen(_session);

	ProcessResize();
}

UI::~UI()
{
	endwin();
}

void UI::ProcessResize()
{
	_screen->ProcessResize();
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

	Screen *newScreen = _screen->ProcessEvent(event);
	refresh();

	if (newScreen == _screen) {
		return true;
	}

	delete _screen;

	_screen = newScreen;
	_screen->Redraw();

	return _screen;
}
