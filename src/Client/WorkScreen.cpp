#include "WorkScreen.hpp"

#include <curses.h>

#include "LoginScreen.hpp"
#include "AttachmentScreen.hpp"
#include "TextColor.hpp"
#include "../Protocol/ActiveSession.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/Hex.hpp"
#include "../Common/File.hpp"

WorkScreen::WorkScreen(ClientSession *session, VoiceChat *voiceChat) :
	Screen(session),
	_notificationSystem(this, &_controls),
	_chatList(session, &_notificationSystem, &_controls),
	_configFile("storage/" +
		DataToHex(session->PublicKey, KEY_SIZE) + "/talk.conf"),
	_controls(session->PublicKey)
{
	_voiceChat = voiceChat;
	_voiceChat->RegisterProcessor(this);

	_session->Processor = this;
	_overlay = nullptr;
	_activeChat = nullptr;

	InitConfigFile();
	_voiceChat->SetConfigFile(&_configFile);
	_voiceChat->SetControls(&_controls);
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

	if (_session->State == ClientSession::ClientStateUnconnected) {
		attrset(COLOR_PAIR(YELLOW_TEXT));
		addstr("not connected");
	} else if (_session->State ==
		ClientSession::ClientStateInitialWaitForServer)
	{
		attrset(COLOR_PAIR(YELLOW_TEXT));
		addstr("handshake in progress");
	} else {
		attrset(COLOR_PAIR(GREEN_TEXT));
		addstr("connected");
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
	if (_overlay) {
		Screen *overlay = _overlay->ProcessEvent(event);

		if (!overlay) {
			delete _overlay;
			_overlay = nullptr;
		}

		return this;
	}

	bool hasNotification = _notificationSystem.ProcessEvent(event);

	if (hasNotification) {
		return this;
	}

	bool voiceProcessed = _voiceChat->ProcessEvent(event);

	if (voiceProcessed) {
		return this;
	}

	if (event == _controls.WorkConnectKey()) {
		Connect();
		return this;
	}

	if (event == _controls.WorkExitKey()) {
		return nullptr;
	}

	if (event == _controls.VoiceEnterSettingsKey()) {
		_voiceChat->StartSettings();
		return this;
	}

	if (!_activeChat) {
		ProcessChatListEvent(event);
	} else if (!_activeChat->Typing()) {
		ProcessChatScreenEvent(event);
	} else {
		if (event == _controls.WorkTypeAttachKey()) {
			_overlay = new AttachmentScreen(
				_activeChat,
				false,
				&_controls);
			return this;
		}

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

void WorkScreen::DeliverMessage(CowBuffer<uint8_t> message)
{
	_chatList.DeliverMessage(message);
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

	_overlay = new LoginScreen(_session, &_configFile, &_controls);
}

void WorkScreen::ProcessChatListEvent(int event)
{
	if (event == _controls.WorkListUpdateKey()) {
		bool res = _session->RequestUserList();

		if (!res) {
			_notificationSystem.Notify(
				"Failed to request user list.");
		}
	} else if (event == _controls.WorkListUpKey()) {
		_chatList.SwitchUp();
	} else if (event == _controls.WorkListDownKey()) {
		_chatList.SwitchDown();
	} else if (event == _controls.WorkListSelectKey()) {
		_activeChat = _chatList.GetCurrentChat();
	}
}

void WorkScreen::ProcessChatScreenEvent(int event)
{
	if (event == _controls.WorkChatBackKey()) {
		_activeChat = nullptr;
	} else if (event == _controls.WorkChatTypeKey()) {
		_activeChat->StartTyping();
	} else if (event == _controls.WorkChatUpKey()) {
		_activeChat->SwitchUp();
	} else if (event == _controls.WorkChatDownKey()) {
		_activeChat->SwitchDown();
	} else if (event == _controls.VoiceStartKey()) {
		VoiceInit(_activeChat->GetPeerKey());
	} else if (event == _controls.WorkChatExtractKey()) {
		if (!_activeChat->HasAttachment()) {
			_notificationSystem.Notify(
				"Selected message does not have an "
				"attachment.");
		} else {
			_overlay = new AttachmentScreen(
				_activeChat,
				true,
				&_controls);
		}
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

	first = new Item("Exit: " + _controls.WorkExitName());
	last = first;

	if (!_session->Connected()) {
		last = last->Add("Connect: " + _controls.WorkConnectName());
	}

	if (!_activeChat) {
		last = last->Add("Select: " + _controls.WorkListSelectName());
		last = last->Add("Scroll: " +
			_controls.WorkListUpName() + "/" +
			_controls.WorkListDownName());
		last = last->Add("Update: " + _controls.WorkListUpdateName());
	} else if (!_activeChat->Typing()) {
		last = last->Add("Type: " + _controls.WorkChatTypeName());
		last = last->Add("Scroll: " +
			_controls.WorkChatUpName() + "/" +
			_controls.WorkChatDownName());
		last = last->Add("Back: " + _controls.WorkChatBackName());

		if (!_voiceChat->Active()) {
			last = last->Add("Voice: " +
				_controls.VoiceStartName());
		}
	} else {
		last = last->Add("Back: " + _controls.WorkTypeBackName());
		last = last->Add("Send: " + _controls.WorkTypeSendName());
		last = last->Add("Attach: " + _controls.WorkTypeAttachName());
	}

	if (_voiceChat->Active()) {
		last = last->Add("End voice: " + _controls.VoiceEndName());
		last = last->Add("Mute: " + _controls.VoiceMuteName());
	}

	last = last->Add("Voice settings: " +
		_controls.VoiceEnterSettingsName());

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
				value = " | " + value;
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
			"storage/" +
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
