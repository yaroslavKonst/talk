#include "AccountScreen.hpp"

#include <curses.h>

#include "TextColor.hpp"
#include "UiLayout.hpp"

AccountScreen::AccountScreen(Root *root)
{
	_root = root;

	_name.Caption = "User name: ";
	_name.SetText(_root->Conf->GetName());
	_originalName = _root->Conf->GetName();

	_receivedAccountSettingsFromServer = false;

	_onlyContactsCanWriteMessages = false;
	_onlyContactsCanCall = false;
	_showInContactList = false;

	_state = State::NameSetting;

	bool success = _root->Network->SetAccountSettingsProcessor(this);

	if (success) {
		_root->Network->RequestAccountSettings();
	}
}

AccountScreen::~AccountScreen()
{
	_root->Network->SetAccountSettingsProcessor(nullptr);

	String newName = _name.GetText();

	if (newName != _originalName) {
		_root->Conf->SetName(newName);
		_root->Conf->Save();

		bool connectionIsActive =
			_root->Network->HandshakeActive() ||
			_root->Network->ConnectionActive();

		if (connectionIsActive) {
			_root->Network->Disconnect();
			_root->Ui->Notify("Account name was changed. "
				"Disconnected from server.");
		}
	}
}

void AccountScreen::Redraw()
{
	for (int i = 0; i < _columns; i++) {
		move(LayoutConstants::HeaderHeight, i);
		addch(ACS_HLINE);
	}

	bool running;
	UiHelpers::DrawFrame(
		LayoutConstants::HeaderHeight + 1,
		_rows - 1 - LayoutConstants::FooterHeight,
		1,
		_columns - 2,
		"Account settings",
		COLOR_PAIR(YELLOW_TEXT),
		running);

	if (running) {
		_root->Ui->RequestRunningLine();
	}

	_name.SetCaptionPosition(_rows / 2 - 3, 3);
	_name.SetWidthLimit(_columns - 6);
	_name.AlignTextToCaption();
	_name.Redraw();

	move(_rows / 2 - 1, 3);

	if (!_receivedAccountSettingsFromServer) {
		running = UiHelpers::DrawRunningLine(
			"Other settings require connection to server.",
			_columns - 6);

		if (running) {
			_root->Ui->RequestRunningLine();
		}

		_state = State::NameSetting;
		_name.SetCursor();
		return;
	}

	String paramString = "Ban all messages not from contact list: ";
	if (_onlyContactsCanWriteMessages) {
		paramString += "yes.";
	} else {
		paramString += "no.";
	}

	running = UiHelpers::DrawRunningLine(paramString, _columns - 6);

	if (running) {
		_root->Ui->RequestRunningLine();
	}

	move(_rows / 2 + 1, 3);
	paramString = "Ban all calls not from contact list: ";
	if (_onlyContactsCanCall) {
		paramString += "yes.";
	} else {
		paramString += "no.";
	}

	running = UiHelpers::DrawRunningLine(paramString, _columns - 6);

	if (running) {
		_root->Ui->RequestRunningLine();
	}

	move(_rows / 2 + 3, 3);
	paramString = "Show this account in public user list: ";
	if (_showInContactList) {
		paramString += "yes.";
	} else {
		paramString += "no.";
	}

	running = UiHelpers::DrawRunningLine(paramString, _columns - 6);

	if (running) {
		_root->Ui->RequestRunningLine();
	}

	if (_state == State::NameSetting) {
		_name.SetCursor();
	} else if (_state == State::MessageSetting) {
		move(_rows / 2 - 1, 2);
	} else if (_state == State::CallSetting) {
		move(_rows / 2 + 1, 2);
	} else if (_state == State::ContactListSetting) {
		move(_rows / 2 + 3, 2);
	}
}

static bool ValidNameCharacter(int event)
{
	return (event >= ' ' && event < 0xff) || event == '\b';
}

Screen *AccountScreen::ProcessEvent(int event)
{
	if (event == _root->Conf->AccountBackKey()) {
		return nullptr;
	}

	if (event == _root->Conf->AccountUpKey()) {
		if (_state == State::MessageSetting) {
			_state = State::NameSetting;
		} else if (_state == State::CallSetting) {
			_state = State::MessageSetting;
		} else if (_state == State::ContactListSetting) {
			_state = State::CallSetting;
		}

		return this;
	}

	if (event == _root->Conf->AccountDownKey()) {
		if (_state == State::NameSetting) {
			_state = State::MessageSetting;
		} else if (_state == State::MessageSetting) {
			_state = State::CallSetting;
		} else if (_state == State::CallSetting) {
			_state = State::ContactListSetting;
		}

		return this;
	}

	if (event == _root->Conf->AccountEnterKey()) {
		bool messages = _onlyContactsCanWriteMessages;
		bool calls = _onlyContactsCanCall;
		bool list = _showInContactList;

		if (_state == State::MessageSetting) {
			messages = !messages;
		} else if (_state == State::CallSetting) {
			calls = !calls;
		} else if (_state == State::ContactListSetting) {
			list = !list;
		} else {
			return this;
		}

		bool success = _root->Network->SetAccountSettings(
			messages,
			calls,
			list);

		if (!success) {
			_root->Ui->Notify("No connection.");
		} else {
			_root->Network->RequestAccountSettings();
		}

		return this;
	}

	if (_state != State::NameSetting) {
		return this;
	}

	if (!ValidNameCharacter(event)) {
		_root->Ui->Notify("Invalid character.");
		return this;
	}

	_name.ProcessChar(event);

	return this;
}

CowBuffer<String> AccountScreen::GetControlHelp()
{
#warning TODO: control help.
	CowBuffer<String> result(1);
	result[0] = "Not implemented.";

	return result;
}

void AccountScreen::ReceiveAccountSettings(
	bool allowMessagesOnlyFromContactList,
	bool allowCallsOnlyFromContactList,
	bool showInContactList)
{
	_receivedAccountSettingsFromServer = true;

	_onlyContactsCanWriteMessages = allowMessagesOnlyFromContactList;
	_onlyContactsCanCall = allowCallsOnlyFromContactList;
	_showInContactList = showInContactList;

	_root->Ui->Redraw();
}
