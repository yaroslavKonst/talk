#include "WorkScreen.hpp"

#include <curses.h>

#include "LoginScreen.hpp"
#include "TextColor.hpp"
#include "../Protocol/ActiveSession.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/Hex.hpp"
#include "../Common/File.hpp"

WorkScreen::WorkScreen(ClientSession *session, VoiceChat *voiceChat) :
	Screen(session),
	_notificationSystem(this),
	_chatList(session, &_notificationSystem),
	_configFile(String("storage/") +
		DataToHex(session->PublicKey, KEY_SIZE) + "/talk.conf")
{
	_voiceChat = voiceChat;
	_voiceChat->RegisterProcessor(this);

	_session->Processor = this;
	_overlay = nullptr;
	_activeChat = nullptr;

	InitConfigFile();
	_voiceChat->SetConfigFile(&_configFile);
}

WorkScreen::~WorkScreen()
{
	_session->Processor = nullptr;

	if (_overlay) {
		delete _overlay;
		_overlay = nullptr;
	}
}

void WorkScreen::Redraw()
{
	if (_overlay) {
		_overlay->ProcessResize();
		return;
	}

	ClearScreen();

	DrawHelp();

	move(1, 0);
	addstr("Connection status: ");

	if (_session->Connected()) {
		attrset(COLOR_PAIR(GREEN_TEXT));
		addstr("connected");
	} else {
		attrset(COLOR_PAIR(YELLOW_TEXT));
		addstr("not connected");
	}

	attrset(COLOR_PAIR(DEFAULT_TEXT));
	addstr(".");

	move(2, _columns / 4);
	addch(ACS_TTEE);

	move(_rows - 2, _columns / 4);
	addch(ACS_BTEE);

	for (int i = 3; i < _rows - 2; i++) {
		move(i, _columns / 4);

		if (i != _rows - 8) {
			addch(ACS_VLINE);
		} else {
			addch(ACS_LTEE);
		}
	}

	for (int i = 0; i < _columns; i++) {
		if (i == _columns / 4) {
			continue;
		}

		move(2, i);
		addch(ACS_HLINE);

		move(_rows - 2, i);
		addch(ACS_HLINE);

		if (i > _columns / 4) {
			move(_rows - 8, i);
			addch(ACS_HLINE);
		}
	}

	_chatList.Redraw(_rows, _columns);

	if (_activeChat) {
		_activeChat->Redraw(_rows, _columns);
	}

	_voiceChat->Redraw(_rows, _columns);
	_notificationSystem.Redraw();

	refresh();
}

Screen *WorkScreen::ProcessEvent(int event)
{
	bool hasNotification = _notificationSystem.ProcessEvent(event);

	if (hasNotification) {
		return this;
	}

	bool voiceProcessed = _voiceChat->ProcessEvent(event);

	if (voiceProcessed) {
		return this;
	}

	if (_overlay) {
		Screen *overlay = _overlay->ProcessEvent(event);

		if (!overlay) {
			delete _overlay;
			_overlay = nullptr;
		}

		return this;
	}

	if (event == KEY_HOME) {
		Connect();
		return this;
	}

	if (event == KEY_END) {
		return nullptr;
	}

	if (event == 'n' - 'a' + 1) {
		_voiceChat->StartSettings();
		return this;
	}

	if (!_activeChat) {
		ProcessChatListEvent(event);
	} else if (!_activeChat->Typing()) {
		ProcessChatScreenEvent(event);
	} else {
		_activeChat->ProcessTyping(event);
	}

	return this;
}

void WorkScreen::NotifyDelivery(void *userPointer, int32_t status)
{
	MessageDescriptor *message = static_cast<MessageDescriptor*>(
		userPointer);

	message->SendInProcess = false;

	if (status == SESSION_RESPONSE_OK) {
		message->SetSent(true);
		Redraw();
		return;
	}

	bool hardFault =
		status == SESSION_RESPONSE_ERROR_INVALID_USER ||
		status == SESSION_RESPONSE_ERROR_MESSAGE_TOO_SHORT;

	if (hardFault) {
		message->SetSendFailure(true);
		message->SetSent(true);
	}

	if (status == SESSION_RESPONSE_ERROR_INVALID_USER) {
		_notificationSystem.Notify(
			"Sent message contains invalid user public key.");
	} else if (status == SESSION_RESPONSE_ERROR_MESSAGE_TOO_SHORT) {
		_notificationSystem.Notify("Sent message has invalid format.");
	} else if (status == SESSION_RESPONSE_ERROR_CONNECTION_LOST) {
		_notificationSystem.Notify(
			"Connection lost. Message was not sent.");
	} else {
		_notificationSystem.Notify(
			"Unknown error code on sent message.");
	}

	Redraw();
}

void WorkScreen::DeliverMessage(CowBuffer<uint8_t> header)
{
	_chatList.DeliverMessage(header);
	Redraw();
}

void WorkScreen::UpdateUserData(const uint8_t *key, String name)
{
	_chatList.UpdateUserData(key, name);
	Redraw();
}

void WorkScreen::VoiceInit(const uint8_t *key)
{
	if (_voiceChat->Active()) {
		_notificationSystem.Notify("Voice chat is already active.");
		return;
	}

	int64_t timestamp = GetUnixTime();

	bool res = _session->InitVoice(key, timestamp);

	if (!res) {
		_notificationSystem.Notify("Failed to send voice request.");
		return;
	}

	_voiceChat->Prepare(
		_chatList.GetUserNameByKey(key),
		key,
		timestamp,
		false,
		_session->PrivateKey,
		_session->PublicKey);
}

void WorkScreen::VoiceRequest(const uint8_t *key, int64_t timestamp)
{
	if (_voiceChat->Active()) {
		_session->ResponseVoiceRequest(false);
		return;
	}

	_voiceChat->Prepare(
		_chatList.GetUserNameByKey(key),
		key,
		timestamp,
		true,
		_session->PrivateKey,
		_session->PublicKey);
	_voiceChat->Ask();
	Redraw();
}

void WorkScreen::VoiceInitResponse(int32_t code)
{
	if (code == SESSION_RESPONSE_ERROR_YOU_IN_VOICE) {
		_notificationSystem.Notify("You are in voice on server side.");
		_voiceChat->Stop();
	} else if (code == SESSION_RESPONSE_ERROR_INVALID_USER) {
		_notificationSystem.Notify("Called user ID does not exist.");
		_voiceChat->Stop();
	} else if (code == SESSION_RESPONSE_ERROR_USER_OFFLINE) {
		_notificationSystem.Notify("Called user is offline.");
		_voiceChat->Stop();
	} else if (code == SESSION_RESPONSE_ERROR_USER_IN_VOICE) {
		_notificationSystem.Notify("Called user is in other call.");
		_voiceChat->Stop();
	} else if (code == SESSION_RESPONSE_VOICE_RINGING) {
		_voiceChat->Wait();
	} else if (code == SESSION_RESPONSE_VOICE_ACCEPT) {
		_voiceChat->Start();
	} else if (code == SESSION_RESPONSE_VOICE_DECLINE) {
		_notificationSystem.Notify("Call declined.");
		_voiceChat->Stop();
	} else {
		_notificationSystem.Notify(
			"Unknown error code in voice request.");
		_voiceChat->Stop();
	}

	Redraw();
}

// Received voice end.
void WorkScreen::VoiceEnd()
{
	_voiceChat->Stop();
	Redraw();
}

// Voice end initiated locally.
void WorkScreen::EndVoice()
{
	bool res = _session->EndVoice();

	if (!res) {
		_notificationSystem.Notify("Failed to send voice end message.");
	}
}

void WorkScreen::VoiceRedrawRequested()
{
	Redraw();
}

void WorkScreen::ReceiveVoiceFrame(CowBuffer<uint8_t> frame)
{
	bool res = _voiceChat->ReceiveVoiceFrame(frame);

	if (!res) {
		_notificationSystem.Notify("Voice stream failure.");
		Redraw();
	}
}

void WorkScreen::SendVoiceFrame(CowBuffer<uint8_t> frame)
{
	bool res = _session->SendVoiceFrame(frame);

	if (!res) {
		_notificationSystem.Notify("Failed to send voice frame.");
		_voiceChat->Stop();
		Redraw();
	}
}

void WorkScreen::AnswerVoiceRequest(bool accept)
{
	bool res = _session->ResponseVoiceRequest(accept);

	if (!res) {
		_notificationSystem.Notify("Failed to send voice response.");
		_voiceChat->Stop();
		return;
	}
}

void WorkScreen::Connect()
{
	if (_session->Connected() || _overlay) {
		_notificationSystem.Notify("Connection is already active.");
		return;
	}

	_overlay = new LoginScreen(_session, &_configFile);
}

void WorkScreen::ProcessChatListEvent(int event)
{
	if (event == 'u' - 'a' + 1) {
		// Ctrl-U.
		bool res = _session->RequestUserList();

		if (!res) {
			_notificationSystem.Notify(
				"Failed to request user list.");
		}
	} else if (event == KEY_UP) {
		_chatList.SwitchUp();
	} else if (event == KEY_DOWN) {
		_chatList.SwitchDown();
	} else if (event == KEY_ENTER || event == '\n') {
		_activeChat = _chatList.GetCurrentChat();

		if (_activeChat) {
			_activeChat->MarkRead();
		}
	}
}

void WorkScreen::ProcessChatScreenEvent(int event)
{
	if (event == '\e') {
		_activeChat->MarkRead();
		_activeChat = nullptr;
	} else if (event == KEY_ENTER || event == '\n') {
		_activeChat->StartTyping();
	} else if (event == KEY_UP) {
		_activeChat->SwitchUp();
	} else if (event == KEY_DOWN) {
		_activeChat->SwitchDown();
	} else if (event == 'v' - 'a' + 1) {
		VoiceInit(_activeChat->GetPeerKey());
	}
}

void WorkScreen::DrawHelp()
{
	struct Item
	{
		Item *Next;
		String Value;

		Item(String value)
		{
			Value = value;
			Next = nullptr;
		}

		~Item()
		{
			if (Next) {
				delete Next;
			}
		}

		Item *Add(String value)
		{
			Next = new Item(value);
			return Next;
		}
	};

	Item *first = nullptr;
	Item *last = nullptr;

	first = new Item("Exit: End");
	last = first;

	if (!_session->Connected()) {
		last = last->Add("Connect: Home");
	}

	if (!_activeChat) {
		last = last->Add("Select: Enter");
		last = last->Add("Scroll: Up/Down");
		last = last->Add("Update: Ctrl-U");
	} else if (!_activeChat->Typing()) {
		last = last->Add("Type: Enter");
		last = last->Add("Scroll: Up/Down");
		last = last->Add("Back: Escape");

		if (!_voiceChat->Active()) {
			last = last->Add("Voice: Ctrl-V");
		}
	} else {
		last = last->Add("Back: Escape");
		last = last->Add("Send: Ctrl-S");
	}

	if (_voiceChat->Active()) {
		last = last->Add("End voice: Ctrl-V");
		last = last->Add("Mute: Ctrl-B");
	}

	last = last->Add("Voice settings: Ctrl-N");

	bool firstLine = true;
	bool firstEntry = true;
	int offset = 0;

	for (Item *it = first; it; it = it->Next) {
		String value = it->Value;

		if (firstLine) {
			if (value.Length() + 3 > _columns - offset) {
				firstLine = false;
				firstEntry = true;
				offset = 0;
			}
		}

		if (!firstEntry) {
			if (firstLine) {
				value = String(" | ") + value;
			} else {
				value = value + " | ";
			}
		}

		firstEntry = false;

		if (firstLine) {
			move(0, offset);
			addstr(value.CStr());
		} else {
			move(1, _columns - offset - value.Length());
			addstr(value.CStr());
		}

		offset += value.Length();
	}

	delete first;
}

void WorkScreen::InitConfigFile()
{
	if (!FileExists(_configFile.GetPath())) {
		CreateDirectory("storage");
		CreateDirectory(
			String("storage/") +
			DataToHex(_session->PublicKey, KEY_SIZE));

		_configFile.Set("connection", "ServerIP", "");
		_configFile.Set("connection", "ServerPort", "6524");
		_configFile.Set("connection", "ServerKey", "");

		_configFile.Set("voice", "Volume", "100");
		_configFile.Set("voice", "ApplyFilter", "Yes");
		_configFile.Set("voice", "SilenceLevel", "3");
		_configFile.Write();
	}
}
