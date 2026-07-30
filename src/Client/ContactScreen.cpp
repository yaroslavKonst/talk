#include "ContactScreen.hpp"

#include <curses.h>

#include "WorkScreen.hpp"
#include "TextColor.hpp"
#include "ContactManageScreen.hpp"
#include "ContactListScreen.hpp"

ContactScreen::ContactScreen(Root *root, WorkScreen *workScreen)
{
	_root = root;
	_workScreen = workScreen;
	_contacts = root->Messages->GetContactStorage();
	_currentContact = _contacts->GetFirstContact();

	_mode = Mode::List;

	_newContactName.Caption = "New contact name: ";
}

ContactScreen::~ContactScreen()
{
}

void ContactScreen::Redraw()
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
		"Contacts",
		COLOR_PAIR(YELLOW_TEXT));

	RedrawContactList();

	if (_mode == Mode::Add) {
		DrawAddWindow();
	}
}

Screen *ContactScreen::ProcessEvent(int event)
{
	if (_mode == Mode::List) {
		return ProcessListEvent(event);
	} else if (_mode == Mode::Add) {
		return ProcessAddEvent(event);
	}

	return this;
}

CowBuffer<String> ContactScreen::GetControlHelp()
{
	if (_mode == Mode::List) {
		CowBuffer<String> result(7);
		result[0] = "Back: " + _root->Conf->ContactBackName();
		result[1] = "Up/Down: " + _root->Conf->ContactUpName() + "/" +
			_root->Conf->ContactDownName();
		result[2] = "Select: " + _root->Conf->ContactEnterName();
		result[3] = "New: " + _root->Conf->ContactNewName();
		result[4] = "Go to chat: " + _root->Conf->ContactToChatName();
		result[5] = "Block/Unblock: " + _root->Conf->ContactBlockName();
		result[6] = "Get contact list: " +
			_root->Conf->ContactListContactsName();

		return result;
	} else if (_mode == Mode::Add) {
		CowBuffer<String> result(2);
		result[0] = "Back: " + _root->Conf->ContactBackName();
		result[1] = "Create contact: " +
			_root->Conf->ContactEnterName();

		return result;
	}

	THROW("Must never be reached.");
}

void ContactScreen::RedrawContactList()
{
	int size = _rows - 9;

	CowBuffer<String> names = _contacts->GetContactRange(
		_currentContact,
		size);

	if (!_currentContact.Length() && names.Size()) {
		_currentContact = names[0];
	}

	int currentContactPosition = 6;

	for (unsigned int i = 0; i < names.Size(); i++) {
		move(i + 6, 3);

		if (names[i] == _currentContact) {
			attrset(COLOR_PAIR(YELLOW_TEXT));
			currentContactPosition = i + 6;
		}

		addstr(names[i].CStr());

		if (names[i] == _currentContact) {
			attrset(COLOR_PAIR(DEFAULT_TEXT));
		}

		Contact::BlockStatus blocked =
			_contacts->GetContact(names[i])->GetBlockStatus();

		if (blocked == Contact::BlockStatus::Blocked) {
			attrset(COLOR_PAIR(RED_TEXT));
			addstr(" Blocked");
			attrset(COLOR_PAIR(DEFAULT_TEXT));
		} else if (blocked == Contact::BlockStatus::SilentlyBlocked) {
			attrset(COLOR_PAIR(RED_TEXT));
			addstr(" Silently Blocked");
			attrset(COLOR_PAIR(DEFAULT_TEXT));
		}
	}

	move(currentContactPosition, 2);
}

Screen *ContactScreen::ProcessListEvent(int event)
{
	if (event == _root->Conf->ContactBackKey()) {
		return nullptr;
	}

	if (event == _root->Conf->ContactUpKey()) {
		String nextContact =
			_contacts->GetPreviousContact(_currentContact);

		if (nextContact.Length()) {
			_currentContact = nextContact;
		}

		return this;
	}

	if (event == _root->Conf->ContactDownKey()) {
		String nextContact =
			_contacts->GetNextContact(_currentContact);

		if (nextContact.Length()) {
			_currentContact = nextContact;
		}

		return this;
	}

	if (event == _root->Conf->ContactEnterKey()) {
		if (!_currentContact.Length()) {
			_root->Ui->Notify("No contacts. Nothing to do.");
			return this;
		}

		return new ContactManageScreen(_root, _currentContact);
	}

	if (event == _root->Conf->ContactNewKey()) {
		_mode = Mode::Add;
		return this;
	}

	if (event == _root->Conf->ContactToChatKey()) {
		if (!_currentContact.Length()) {
			_root->Ui->Notify("No contacts. Nothing to do.");
			return this;
		}

		_root->Messages->SelectOrCreateChat(_currentContact);
		_workScreen->ActivateCurrentChat();
		return nullptr;
	}

	if (event == _root->Conf->ContactBlockKey()) {
		if (!_currentContact.Length()) {
			_root->Ui->Notify("No contacts. Nothing to do.");
			return this;
		}

		Contact *contact = _contacts->GetContact(_currentContact);
		Contact::BlockStatus blocked = contact->GetBlockStatus();

		if (blocked == Contact::BlockStatus::Allowed) {
			blocked = Contact::BlockStatus::Blocked;
		} else if (blocked == Contact::BlockStatus::Blocked) {
			blocked = Contact::BlockStatus::SilentlyBlocked;
		} else if (blocked == Contact::BlockStatus::SilentlyBlocked) {
			blocked = Contact::BlockStatus::Allowed;
		}

		bool requestSuccess = _root->Network->BlockContact(
			_currentContact,
			blocked);

		if (!requestSuccess) {
			_root->Ui->Notify(
				"Failed to set contact properties. "
				"No connection.");
		}

		return this;
	}

	if (event == _root->Conf->ContactListContactsKey()) {
		if (!_root->Network->ConnectionActive()) {
			_root->Ui->Notify("No connection.");
			return this;
		}

		return new ContactListScreen(_root);
	}

	return this;
}

void ContactScreen::DrawAddWindow()
{
	int baseY = _rows / 2;
	int baseX = _columns / 2;

	int xOffset =
		(_newContactName.Caption.Length() +
		_newContactName.Text.Length()) /
		2;

	UiHelpers::ClearScreen(
		baseY - 3,
		baseY + 3,
		baseX - xOffset - 3,
		baseX + xOffset + 3);

	UiHelpers::DrawFrame(
		baseY - 2,
		baseY + 2,
		baseX - xOffset - 2,
		baseX + xOffset + 2,
		"New contact",
		COLOR_PAIR(YELLOW_TEXT));

	_newContactName.SetCaptionPosition(baseY, baseX - xOffset);
	_newContactName.AlignTextToCaption();
	_newContactName.Redraw();
}

static bool ValidChar(int event)
{
	return (event > ' ' && event <= '~') || event == '\b';
}

Screen *ContactScreen::ProcessAddEvent(int event)
{
	if (event == _root->Conf->ContactBackKey()) {
		_mode = Mode::List;
		return this;
	}

	if (event == _root->Conf->ContactEnterKey()) {
		if (!_newContactName.Text.Length()) {
			_root->Ui->Notify("Contact name must not be empty.");
			return this;
		}

		if (_contacts->GetContact(_newContactName.Text)) {
			_root->Ui->Notify("Contact already exists.");
			return this;
		}

		bool requestSuccess = _root->Network->AddContact(
			_newContactName.Text);

		if (!requestSuccess) {
			_root->Ui->Notify(
				"Failed to add new contact. No connection.");
			return this;
		}

		_newContactName.Text = "";
		_mode = Mode::List;
		return this;
	}

	if (ValidChar(event)) {
		_newContactName.ProcessChar(event);
	} else {
		_root->Ui->Notify("Invalid character.");
	}

	return this;
}
