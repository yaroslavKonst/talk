#include "UI.hpp"

#include <locale.h>
#include <curses.h>

#include "TextColor.hpp"
#include "WorkScreen.hpp"
#include "UiHelpers.hpp"
#include "../Common/Exception.hpp"
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
	keypad(stdscr, 1);
	start_color();

	init_pair(GREEN_TEXT, COLOR_GREEN, COLOR_BLACK);
	init_pair(YELLOW_TEXT, COLOR_YELLOW, COLOR_BLACK);
	init_pair(RED_TEXT, COLOR_RED, COLOR_BLACK);

	_screenStack = new ScreenStackEntry(nullptr);
	_screenStack->screen = new WorkScreen(_root);

	ProcessResize();

	_root->Dispatcher->RegisterDescriptorProcessor(this);
}

UI::~UI()
{
	_root->Dispatcher->UnregisterDescriptorProcessor(this);
	endwin();
}

void UI::ProcessRead()
{
	bool res = ProcessEvent();

	if (!res) {
		_root->Dispatcher->Stop();
	}
}

bool UI::ProcessEvent()
{
	int event = getch();

	if (event == KEY_RESIZE) {
		ProcessResize();
		return true;
	}

	if (_rows < 24 || _columns < 80) {
		return true;
	}

	bool notificationProcessed = _notifier.ProcessEvent(event);

	if (notificationProcessed) {
		Redraw();
		return true;
	}

	if (event == KEY_ENTER) {
		event = '\n';
	} else if (event == KEY_BACKSPACE) {
		event = '\b';
	}

	Screen *newScreen = _screenStack->screen->ProcessEvent(event);

	if (newScreen != _screenStack->screen) {
		if (newScreen) {
			_screenStack = new ScreenStackEntry(_screenStack);
			_screenStack->screen = newScreen;
		} else {
			delete _screenStack->screen;
			ScreenStackEntry *tmp = _screenStack;
			_screenStack = _screenStack->previous;
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
	getmaxyx(stdscr, _rows, _columns);

	if (_screenStack) {
		_screenStack->screen->ProcessResize();
	}

	Redraw();
}

void UI::Redraw()
{
	UiHelpers::ClearScreen(0, _rows - 1, 0, _columns - 1);

	if (_rows < 24 || _columns < 80) {
		move(0, 1);
		addstr("Terminal screen is too small.");
		move(0, 0);
		refresh();
		return;
	}

	DrawUserData();
	DrawConnectionState();
	DrawVoiceState();
	DrawControlHelp();

	if (_screenStack) {
		_screenStack->screen->Redraw();
	}

	_notifier.Redraw();

	refresh();
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

void UI::DrawUserData()
{
	move(0, 0);
	addstr("Login: ");
	if (_root->Conf->GetName().Length() > 0) {
		addstr(_root->Conf->GetName().CStr());
	} else {
		attrset(COLOR_PAIR(RED_TEXT));
		addstr("not specified");
		attrset(COLOR_PAIR(DEFAULT_TEXT));
	}

	String hostName = _root->Conf->GetHostName();

	if (hostName.Length() > 0) {
		addch('@');
		addstr(hostName.CStr());
	} else {
		attrset(COLOR_PAIR(RED_TEXT));
		addstr(" (host name is not specified)");
		attrset(COLOR_PAIR(DEFAULT_TEXT));
	}

	move(1, 0);
	addstr("Key: ");
	addstr(DataToHex(
		_root->PublicKey->Key,
		Crypto::X25519::KEY_SIZE).CStr());
}

void UI::DrawConnectionState()
{
	move(2, 0);
	addstr("Connection status: ");

	if (_root->Network->ConnectionActive()) {
		attrset(COLOR_PAIR(GREEN_TEXT));
		addstr("connected");
	} else if (_root->Network->HandshakeActive()) {
		attrset(COLOR_PAIR(YELLOW_TEXT));
		addstr("connecting");
	} else {
		attrset(COLOR_PAIR(RED_TEXT));
		addstr("not connected");
	}

	attrset(COLOR_PAIR(DEFAULT_TEXT));
	addch('.');
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
		}

		move(posY, posX);

		if (posX) {
			addstr(" | ");
		}

		addstr(help[i].CStr());

		posX += totalLength;
	}
}
