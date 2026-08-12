#include "UI.hpp"

#include <unistd.h>
#include <locale.h>
#include <curses.h>
#include <sys/ioctl.h>

#include "TextColor.hpp"
#include "WorkScreen.hpp"
#include "UiHelpers.hpp"
#include "../Common/Exception.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/Hex.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

UI::UI(Root *root) :
	_notifier(root)
{
	_root = root;

	setlocale(LC_ALL, "");

	initscr();
	raw();
	noecho();
	wtimeout(stdscr, 100);
	keypad(stdscr, 1);
	start_color();

	init_pair(GREEN_TEXT, COLOR_GREEN, COLOR_BLACK);
	init_pair(YELLOW_TEXT, COLOR_YELLOW, COLOR_BLACK);
	init_pair(RED_TEXT, COLOR_RED, COLOR_BLACK);

	_needRunningString = false;

	_screenStack = new ScreenStackEntry(nullptr);
	_screenStack->screen = new WorkScreen(_root);

	_root->Dispatcher->RegisterDescriptorProcessor(this);
	_root->Dispatcher->RegisterSignalProcessor(this, SIGWINCH);
}

UI::~UI()
{
	_root->Dispatcher->UnregisterSignalProcessor(this, SIGWINCH);
	_root->Dispatcher->UnregisterDescriptorProcessor(this);
	_root->Dispatcher->UnregisterTimeProcessor(this);
	endwin();
}

void UI::ProcessRead()
{
	bool res = ProcessEvent();

	if (!res) {
		_root->Dispatcher->Stop();
	}
}

void UI::ProcessSignal(int signum)
{
	if (signum == SIGWINCH) {
		ProcessResize();
	}
}

bool UI::ProcessEvent()
{
	int event = getch();

	if (event == ERR) {
		return true;
	}

	if (event == KEY_RESIZE) {
		return true;
	}

	if (event == KEY_ENTER) {
		event = '\n';
	} else if (event == KEY_BACKSPACE) {
		event = '\b';
	}

	if (_rows < 20 || _columns < 26) {
		return true;
	}

	bool notificationProcessed = _notifier.ProcessEvent(event);

	if (notificationProcessed) {
		Redraw();
		return true;
	}

	Screen *newScreen = _screenStack->screen->ProcessEvent(event);

	if (newScreen != _screenStack->screen) {
		if (newScreen) {
			_screenStack = new ScreenStackEntry(_screenStack);
			_screenStack->screen = newScreen;
		} else {
			ScreenStackEntry *tmp = _screenStack;
			_screenStack = _screenStack->previous;
			delete tmp->screen;
			delete tmp;
		}
	}

	if (_screenStack) {
		Redraw();
	}

	return _screenStack;
}

void UI::ProcessResize()
{
	struct winsize size;

	int res = ioctl(0, TIOCGWINSZ, &size);

	if (res == -1) {
		THROW("Failed to get new window size.");
	}

	resizeterm(size.ws_row, size.ws_col);
	getmaxyx(stdscr, _rows, _columns);

	ScreenStackEntry *e = _screenStack;

	while (e) {
		e->screen->ProcessResize();
		e = e->previous;
	}

	Redraw();
}

void UI::Redraw()
{
	UiHelpers::ClearScreen(0, _rows - 1, 0, _columns - 1);

	bool hadRunningString = _needRunningString;
	_needRunningString = false;

	if (_rows < 20 || _columns < 26) {
		move(0, 0);
		addstr("Terminal screen is too small.");
		move(1, 0);
	} else {
		DrawUserData();
		DrawConnectionState();
		DrawVoiceState();
		DrawControlHelp();

		if (_screenStack) {
			_screenStack->screen->Redraw();
		}

		_notifier.Redraw();
	}

	refresh();

	if (_needRunningString && !hadRunningString) {
		SetTimestamp(GetMonotonicMillisecondTime());
		SetInterval(500);

		_root->Dispatcher->RegisterTimeProcessor(this);
	} else if (hadRunningString && !_needRunningString) {
		_root->Dispatcher->UnregisterTimeProcessor(this);
	}
}

void UI::Notify(String message)
{
	_notifier.Notify(message);
}

void *UI::BlockNotify(String message)
{
	return _notifier.BlockNotify(message);
}

void UI::BlockCancel(void *handle)
{
	_notifier.BlockCancel(handle);
}

void UI::RequestRunningLine()
{
	_needRunningString = true;
}

void UI::ProcessTimeEvent()
{
	UiHelpers::UpdateRunningLineSeed();
	_root->Ui->Redraw();
}

void UI::DrawUserData()
{
	move(0, 0);
	addstr("Login: ");

	String loginString;
	bool loginValid = true;

	if (_root->Conf->GetName().Length() > 0) {
		loginString += _root->Conf->GetName();
	} else {
		loginString += "not specified";
		loginValid = false;
	}

	String hostName = _root->Conf->GetHostName();

	if (hostName.Length() > 0) {
		loginString += "@" + hostName;
	} else {
		loginString += " (host name is not specified)";
		loginValid = false;
	}

	if (!loginValid) {
		attrset(COLOR_PAIR(RED_TEXT));
	}

	bool running = UiHelpers::DrawRunningLine(
		loginString,
		_columns - 7);

	if (running) {
		RequestRunningLine();
	}

	attrset(COLOR_PAIR(DEFAULT_TEXT));

	move(1, 0);
	addstr("Key: ");
	running = UiHelpers::DrawRunningLine(
		DataToHex(
			_root->PublicKey->Key,
			Crypto::X25519::KEY_SIZE),
		_columns - 5);

	if (running) {
		RequestRunningLine();
	}
}

void UI::DrawConnectionState()
{
	move(2, 0);
	addstr("Connection status: ");

	String line;

	if (_root->Network->ConnectionActive()) {
		attrset(COLOR_PAIR(GREEN_TEXT));
		line = "connected.";
	} else if (_root->Network->HandshakeActive()) {
		attrset(COLOR_PAIR(YELLOW_TEXT));
		line = "connecting.";
	} else {
		attrset(COLOR_PAIR(RED_TEXT));
		line = "not connected.";
	}

	bool running = UiHelpers::DrawRunningLine(line, _columns - 19);

	if (running) {
		RequestRunningLine();
	}

	attrset(COLOR_PAIR(DEFAULT_TEXT));
}

void UI::DrawVoiceState()
{
	move(3, 0);
	addstr("Voice status: ");

/*	VoiceEventProcessor::VoiceState state = _root->Voice->GetState();

	switch (state) {
	case VoiceEventProcessor::VoiceStateOff:
		addstr("not connected.");
		return;
	case VoiceEventProcessor::VoiceStateInit:
		attrset(COLOR_PAIR(YELLOW_TEXT));
		addstr("initializing connection");
		break;
	case VoiceEventProcessor::VoiceStateAsk:
		attrset(COLOR_PAIR(YELLOW_TEXT));
		addstr("please respond");
		break;
	case VoiceEventProcessor::VoiceStateWait:
		attrset(COLOR_PAIR(YELLOW_TEXT));
		addstr("waiting for answer");
		break;
	case VoiceEventProcessor::VoiceStateActive:
		attrset(COLOR_PAIR(GREEN_TEXT));
		addstr("active");
		break;
	}

	String name = _root->Voice->GetPeerName();

	if (name.Length() > 30) {
		name = name.Substring(0, 30) + "...";
	}

	addstr((" (" + name + ")").CStr());

	if (state == VoiceEventProcessor::VoiceStateActive) {
		if (_root->Voice->IsMuted()) {
			attrset(COLOR_PAIR(RED_TEXT));
			addstr(" (mute)");
		} else if (_root->Voice->IsSilent()) {
			addstr(" (silence)");
		}
	}

	attrset(COLOR_PAIR(DEFAULT_TEXT));*/
	addch('.');
}

void UI::DrawControlHelp()
{
	if (!_screenStack) {
		return;
	}

	int posY = _rows - 1;
	int posX = 0;

	CowBuffer<String> help = _screenStack->screen->GetControlHelp();

	for (unsigned int i = 0; i < help.Size(); i++) {
		int totalLength = help[i].Length();

		if (posX) {
			totalLength += 3;
		}

		if (posX + totalLength > _columns) {
			if (posX) {
				totalLength -= 3;
			}

			posX = 0;
			--posY;

			if (posY < _rows - 2) {
				break;
			}
		}

		move(posY, posX);

		if (posX) {
			addstr(" | ");
		}

		addstr(help[i].CStr());

		posX += totalLength;
	}
}
