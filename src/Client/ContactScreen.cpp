#include "ContactScreen.hpp"

#include <curses.h>

#include "WorkScreen.hpp"
#include "TextColor.hpp"
#include "ContactManageScreen.hpp"
#include "ContactListScreen.hpp"
#include "UiLayout.hpp"

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
		move(LayoutConstants::HeaderHeight, i);
		addch(ACS_HLINE);
	}

	bool running;
	UiHelpers::DrawFrame(
		LayoutConstants::HeaderHeight + 1,
		_rows - 1 - LayoutConstants::FooterHeight,
		1,
		_columns - 2,
		"Contacts",
		COLOR_PAIR(YELLOW_TEXT),
		running);

	if (running) {
		_root->Ui->RequestRunningLine();
	}

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
		CowBuffer<String> result(8);
		result[0] = "Back: " + _root->Conf->ContactBackName();
		result[1] = "Up/Down: " + _root->Conf->ContactUpName() + "/" +
			_root->Conf->ContactDownName();
		result[2] = "Select: " + _root->Conf->ContactEnterName();
		result[3] = "New: " + _root->Conf->ContactNewName();
		result[4] = "Go to chat: " + _root->Conf->ContactToChatName();
		result[5] = "Block/Unblock: " + _root->Conf->ContactBlockName();
		result[6] = "Remove: " + _root->Conf->ContactRemoveName();
		result[7] = "Get contact list: " +
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
	int size = _rows -
		LayoutConstants::HeaderHeight -
		LayoutConstants::FooterHeight - 3;

	if (!_contacts->HasContact(_currentContact)) {
		_currentContact = String();
	}

	CowBuffer<String> names = _contacts->GetContactRange(
		_currentContact,
		size);

	if (!_currentContact.Length() && names.Size()) {
		_currentContact = names[0];
	}

	int currentContactPosition = LayoutConstants::HeaderHeight + 2;

	for (unsigned int i = 0; i < names.Size(); i++) {
		move(i + LayoutConstants::HeaderHeight + 2, 3);

		String nameString = names[i];
		String blockStatusString;

		Contact::BlockStatus blocked =
			_contacts->GetContact(names[i])->GetBlockStatus();

		if (blocked == Contact::BlockStatus::Blocked) {
			blockStatusString = "Blocked";
		} else if (blocked == Contact::BlockStatus::SilentlyBlocked) {
			blockStatusString = "Silently Blocked";
		}

		int textAttr = COLOR_PAIR(DEFAULT_TEXT);

		if (names[i] == _currentContact) {
			currentContactPosition = i +
				LayoutConstants::HeaderHeight + 2;
			textAttr = COLOR_PAIR(YELLOW_TEXT);
		}

		int widthLimit = _columns - 6;

		bool running = UiHelpers::DrawCommentedLine(
			nameString,
			blockStatusString,
			widthLimit,
			textAttr,
			COLOR_PAIR(RED_TEXT));

		if (running) {
			_root->Ui->RequestRunningLine();
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

	if (event == _root->Conf->ContactRemoveKey()) {
		if (!_currentContact.Length()) {
			_root->Ui->Notify("No contacts. Nothing to do.");
			return this;
		}

		bool requestSuccess = _root->Network->RemoveContact(
			_currentContact);

		if (!requestSuccess) {
			_root->Ui->Notify(
				"Failed to remove contact. "
				"No connection.");
		}

		return this;
	}

	return this;
}

void ContactScreen::DrawAddWindow()
{
	int baseY = _rows / 2;
	int baseX = _columns / 2;

	int xOffset =
		(UTF8::StrLen(_newContactName.Caption.CStr()) +
		_newContactName.GetTextLength()) /
		2;

	if (xOffset > _columns / 2 - 3) {
		xOffset = _columns / 2 - 3;
	}

	UiHelpers::ClearScreen(
		baseY - 3,
		baseY + 3,
		baseX - xOffset - 3,
		baseX + xOffset + 3);

	bool running;
	UiHelpers::DrawFrame(
		baseY - 2,
		baseY + 2,
		baseX - xOffset - 2,
		baseX + xOffset + 2,
		"New contact",
		COLOR_PAIR(YELLOW_TEXT),
		running);

	if (running) {
		_root->Ui->RequestRunningLine();
	}

	_newContactName.SetCaptionPosition(baseY, baseX - xOffset);
	_newContactName.SetWidthLimit(_columns - 6);
	_newContactName.AlignTextToCaption();
	_newContactName.Redraw();
}

static bool ValidChar(int event)
{
	return (event > ' ' && event < 0xff) || event == '\b';
}

Screen *ContactScreen::ProcessAddEvent(int event)
{
	if (event == _root->Conf->ContactBackKey()) {
		_mode = Mode::List;
		return this;
	}

	if (event == _root->Conf->ContactEnterKey()) {
		if (!_newContactName.HasText()) {
			_root->Ui->Notify("Contact name must not be empty.");
			return this;
		}

		String newContactName = _newContactName.GetText();

		if (_contacts->GetContact(newContactName)) {
			_root->Ui->Notify("Contact already exists.");
			return this;
		}

		if (!Message::VerifyFullUserName(newContactName)) {
			_root->Ui->Notify("Contact name has invalid format.");
			return this;
		}

		String myName = _root->Conf->GetName() + "@" +
			_root->Conf->GetHostName();

		if (newContactName == myName) {
			_root->Ui->Notify("Contact name is your account name.");
			return this;
		}

		bool requestSuccess = _root->Network->AddContact(
			newContactName);

		if (!requestSuccess) {
			_root->Ui->Notify(
				"Failed to add new contact. No connection.");
			return this;
		}

		_newContactName.SetText("");
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
