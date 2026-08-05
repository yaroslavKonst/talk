#include "AccountScreen.hpp"

#include <curses.h>

#include "TextColor.hpp"

AccountScreen::AccountScreen(Root *root)
{
	_root = root;

	_name.Caption = "User name: ";
	_name.Text = _root->Conf->GetName();
	_originalName = _name.Text;

	_receivedAccountSettingsFromServer = false;
	_onlyContactsCanWriteMessages = false;
	_onlyContactsCanCall = false;

	_state = State::NameSetting;

	bool success = _root->Network->SetAccountSettingsProcessor(this);

	if (success) {
		_root->Network->RequestAccountSettings();
	}
}

AccountScreen::~AccountScreen()
{
	_root->Network->SetAccountSettingsProcessor(nullptr);

	if (_name.Text != _originalName) {
		_root->Conf->SetName(_name.Text);
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
		move(4, i);
		addch(ACS_HLINE);
	}

	UiHelpers::DrawFrame(
		5,
		_rows - 3,
		1,
		_columns - 2,
		"Account settings",
		COLOR_PAIR(YELLOW_TEXT));

	_name.SetCaptionPosition(_rows / 2 - 2, 4);
	_name.AlignTextToCaption();
	_name.Redraw();

	move(_rows / 2, 4);

	if (!_receivedAccountSettingsFromServer) {
		addstr("Other settings require connection to server.");
		_state = State::NameSetting;
		_name.SetCursor();
		return;
	}

	addstr("Ban all messages not from command list: ");
	if (_onlyContactsCanWriteMessages) {
		addstr("yes.");
	} else {
		addstr("no.");
	}

	move(_rows / 2 + 2, 4);
	addstr("Ban all calls not from command list: ");
	if (_onlyContactsCanCall) {
		addstr("yes.");
	} else {
		addstr("no.");
	}

	if (_state == State::NameSetting) {
		_name.SetCursor();
	} else if (_state == State::MessageSetting) {
		move(_rows / 2, 3);
	} else if (_state == State::CallSetting) {
		move(_rows / 2 + 2, 3);
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
		}

		return this;
	}

	if (event == _root->Conf->AccountDownKey()) {
		if (_state == State::NameSetting) {
			_state = State::MessageSetting;
		} else if (_state == State::MessageSetting) {
			_state = State::CallSetting;
		}

		return this;
	}

	if (event == _root->Conf->AccountEnterKey()) {
		bool messages = _onlyContactsCanWriteMessages;
		bool calls = _onlyContactsCanCall;

		if (_state == State::MessageSetting) {
			messages = !messages;
		} else if (_state == State::CallSetting) {
			calls = !calls;
		} else {
			return this;
		}

		bool success = _root->Network->SetAccountSettings(
			messages,
			calls);

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
	bool allowCallsOnlyFromContactList)
{
	_receivedAccountSettingsFromServer = true;

	_onlyContactsCanWriteMessages = allowMessagesOnlyFromContactList;
	_onlyContactsCanCall = allowCallsOnlyFromContactList;

	_root->Ui->Redraw();
}
