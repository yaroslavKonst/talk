#include "UI.hpp"

#include "locale.h"

#include "TextColor.hpp"
#include "PasswordScreen.hpp"

UI::UI(ClientSession *session)
{
	setlocale(LC_ALL, "");

	initscr();
	raw();
	noecho();
	keypad(stdscr, 1);
	start_color();

	init_pair(GREEN_TEXT, COLOR_GREEN, COLOR_BLACK);
	init_pair(YELLOW_TEXT, COLOR_YELLOW, COLOR_BLACK);
	init_pair(RED_TEXT, COLOR_RED, COLOR_BLACK);

	_session = session;
	_screen = new PasswordScreen(_session, &_voiceChat);

	ProcessResize();
}

UI::~UI()
{
	endwin();
}

void UI::ProcessResize()
{
	if (_screen) {
		_screen->ProcessResize();
	}
}

bool UI::ProcessEvent()
{
	int event = getch();

	if (event == KEY_RESIZE) {
		ProcessResize();
		return true;
	}

	if (event == KEY_ENTER) {
		event = '\n';
	} else if (event == KEY_BACKSPACE) {
		event = '\b';
	}

	Screen *newScreen = _screen->ProcessEvent(event);

	if (newScreen) {
		newScreen->Redraw();
	}

	if (newScreen == _screen) {
		return true;
	}

	delete _screen;
	_screen = newScreen;

	return _screen;
}

void UI::Disconnect()
{
	if (!_session->Connected()) {
		return;
	}

	_session->Disconnect();

	if (_voiceChat.Active()) {
		_voiceChat.Stop();
	}

	if (_screen) {
		_screen->Redraw();
	}
}

int UI::GetSoundReadFileDescriptor()
{
	return _voiceChat.GetSoundReadFileDescriptor();
}

void UI::ProcessSound()
{
	_voiceChat.ProcessInput();
}
