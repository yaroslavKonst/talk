#include "UI.hpp"

#include <unistd.h>
#include <locale.h>
#include <curses.h>
#include <sys/ioctl.h>

#include "TextColor.hpp"
#include "WorkScreen.hpp"
#include "UiHelpers.hpp"
#include "UiLayout.hpp"
#include "../Common/Exception.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/Hex.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

enum
{
	MinimumScreenWidth = 26,
	MinimumScreenHeight = 20
};

UI::UI(Root *root) :
	_notifier(root)
{
	try {
		_root = root;

		setlocale(LC_ALL, "");

		initscr();
		raw();
		noecho();
		timeout(0);
		keypad(stdscr, 1);
		start_color();

		init_pair(GREEN_TEXT, COLOR_GREEN, COLOR_BLACK);
		init_pair(YELLOW_TEXT, COLOR_YELLOW, COLOR_BLACK);
		init_pair(RED_TEXT, COLOR_RED, COLOR_BLACK);

		_needRunningString = false;
		_autoConnectStatus = false;

		_screenStack = new ScreenStackEntry(nullptr);
		_screenStack->screen = new WorkScreen(_root);

		_root->Dispatcher->RegisterDescriptorProcessor(this);
		_root->Dispatcher->RegisterSignalProcessor(this, SIGWINCH);

		if (_root->Conf->GetAutoconnect()) {
			_root->Dispatcher->RegisterQuantProcessor(this);
		}
	} catch (...) {
		Cleanup();
		throw;
	}
}

UI::~UI()
{
	Cleanup();
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

	if (_rows < MinimumScreenHeight || _columns < MinimumScreenWidth) {
		return true;
	}

	bool notificationProcessed = _notifier.ProcessEvent(event);

	if (notificationProcessed) {
		Redraw();
		return true;
	}

	bool voiceEventProcessed = ProcessVoiceEvent(event);

	if (voiceEventProcessed) {
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

	if (_rows < MinimumScreenHeight || _columns < MinimumScreenWidth) {
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

		DrawVoiceInterface();
		_notifier.Redraw();
	}

	refresh();

	if (_needRunningString && !hadRunningString) {
		// Redraw moving interface parts every 0.5 seconds.
		SetInterval(500);
		SetTimestamp(GetMonotonicMillisecondTime());

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

void UI::ProcessQuant()
{
	if (!_autoConnectStatus) {
		ungetch(_root->Conf->WorkConnectKey());
		ProcessEvent();
		_root->Dispatcher->RegisterQuantProcessor(this);
		_autoConnectStatus = true;
		return;
	}

	ungetch(_root->Conf->LoginConnectKey());
	ProcessEvent();
	_autoConnectStatus = false;
}

void UI::Cleanup()
{
	_root->Dispatcher->UnregisterSignalProcessor(this, SIGWINCH);
	_root->Dispatcher->UnregisterDescriptorProcessor(this);
	_root->Dispatcher->UnregisterTimeProcessor(this);
	_root->Dispatcher->UnregisterQuantProcessor(this);
	endwin();
}

void UI::DrawUserData()
{
	int userY = 0;
	int keyY = 1;
	String loginCaption = "Login: ";
	String keyCaption = "Key: ";

	move(userY, 0);
	addstr(loginCaption.CStr());

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
		_columns - loginCaption.Length());

	if (running) {
		RequestRunningLine();
	}

	attrset(COLOR_PAIR(DEFAULT_TEXT));

	move(keyY, 0);
	addstr(keyCaption.CStr());
	running = UiHelpers::DrawRunningLine(
		DataToHex(
			_root->PublicKey->Key,
			Crypto::X25519::KEY_SIZE),
		_columns - keyCaption.Length());

	if (running) {
		RequestRunningLine();
	}
}

void UI::DrawConnectionState()
{
	int lineY = 2;
	String caption = "Connection status: ";

	if (_columns < caption.Length() * 2) {
		caption = "Conn stat: ";
	}

	move(lineY, 0);
	addstr(caption.CStr());

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

	bool running = UiHelpers::DrawRunningLine(
		line,
		_columns - caption.Length());

	if (running) {
		RequestRunningLine();
	}

	attrset(COLOR_PAIR(DEFAULT_TEXT));
}

void UI::DrawVoiceState()
{
	int lineY = 3;
	String caption = "Voice status: ";

	if (_columns < caption.Length() * 2) {
		caption = "Voice stat: ";
	}

	move(lineY, 0);
	addstr(caption.CStr());

	VoiceEventProcessor::State state = _root->Voice->GetState();

	String stateString;

	switch (state) {
	case VoiceEventProcessor::State::Closed:
		stateString = "not connected.";
		break;
	case VoiceEventProcessor::State::InitSent:
		attrset(COLOR_PAIR(YELLOW_TEXT));
		stateString = "initializing connection";
		break;
	case VoiceEventProcessor::State::WaitingForUserAnswer:
		attrset(COLOR_PAIR(YELLOW_TEXT));
		stateString = "please respond";
		break;
	case VoiceEventProcessor::State::WaitingForPeerAnswer:
		attrset(COLOR_PAIR(YELLOW_TEXT));
		stateString = "waiting for answer";
		break;
	case VoiceEventProcessor::State::ActiveSession:
		attrset(COLOR_PAIR(GREEN_TEXT));
		stateString = "active";
		break;
	}

	if (state != VoiceEventProcessor::State::Closed) {
		stateString += " (" + _root->Voice->GetPeerName() + ")";

		/*if (state == VoiceEventProcessor::VoiceStateActive) {
			if (_root->Voice->IsMuted()) {
				attrset(COLOR_PAIR(RED_TEXT));
				stateString += " (mute)";
			} else if (_root->Voice->IsSilent()) {
				stateString += " (silence)";
			}
		}*/
	}

	bool running = UiHelpers::DrawRunningLine(
		stateString,
		_columns - caption.Length());

	if (running) {
		RequestRunningLine();
	}

	attrset(COLOR_PAIR(DEFAULT_TEXT));
}

void UI::DrawVoiceInterface()
{
	VoiceEventProcessor::State state = _root->Voice->GetState();

	if (state != VoiceEventProcessor::State::WaitingForUserAnswer) {
		return;
	}

	int baseY = _rows / 2;

	String peerName = _root->Voice->GetPeerName();
	Crypto::X25519::PublicKeyContainer peerKey =
		_root->Voice->GetPeerPublicKey();

	UiHelpers::ClearScreen(baseY - 4, baseY + 4, 0, _columns - 1);

	bool running;
	UiHelpers::DrawFrame(
		baseY - 3,
		baseY + 3,
		1,
		_columns - 2,
		"Inbound call from " + peerName,
		COLOR_PAIR(YELLOW_TEXT),
		running);

	if (running) {
		RequestRunningLine();
	}

	int posY = baseY - 2;
	int posX = (_columns - peerName.Length()) / 2;

	if (posX < 3) {
		posX = 3;
	}

	move(posY, posX);
	running = UiHelpers::DrawRunningLine(peerName, _columns - 6);

	if (running) {
		RequestRunningLine();
	}

	posY += 2;

	Contact *contact = _root->Messages->GetContactStorage()->GetContact(
		peerName);

	String message;

	if (!contact) {
		attrset(COLOR_PAIR(RED_TEXT));
		message = "Not in contacts.";
	} else {
		if (!contact->IsKeyVerified(peerKey)) {
			attrset(COLOR_PAIR(YELLOW_TEXT));
			message = "In contacts, unverified key.";
		} else {
			attrset(COLOR_PAIR(GREEN_TEXT));
			message = "In contacts, verified key.";
		}
	}

	posX = (_columns - message.Length()) / 2;

	if (posX < 3) {
		posX = 3;
	}

	move(posY, posX);
	running = UiHelpers::DrawRunningLine(message, _columns - 6);
	attrset(COLOR_PAIR(DEFAULT_TEXT));

	if (running) {
		RequestRunningLine();
	}

	posY += 2;

	message = "Press " + _root->Conf->VoiceAcceptName() + " to answer, " +
		_root->Conf->VoiceDeclineName() + " to decline call.";

	posX = (_columns - message.Length()) / 2;

	if (posX < 3) {
		posX = 3;
	}

	move(posY, posX);
	running = UiHelpers::DrawRunningLine(message, _columns - 6);

	if (running) {
		RequestRunningLine();
	}
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

			if (posY < _rows - LayoutConstants::FooterHeight) {
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

bool UI::ProcessVoiceEvent(int event)
{
	VoiceEventProcessor::State state = _root->Voice->GetState();

	if (state == VoiceEventProcessor::State::WaitingForUserAnswer) {
		if (event == _root->Conf->VoiceAcceptKey()) {
			_root->Voice->RespondToInboundCall(true);
		} else if (event == _root->Conf->VoiceDeclineKey()) {
			_root->Voice->RespondToInboundCall(false);
		}

		return true;
	}

	if (state != VoiceEventProcessor::State::Closed) {
		if (event == _root->Conf->VoiceEndKey()) {
			_root->Voice->EndCall();
			return true;
		}

		return false;
	}

	return false;
}
