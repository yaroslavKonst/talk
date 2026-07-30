#include "ContactListScreen.hpp"

#include <curses.h>

#include "WorkScreen.hpp"
#include "TextColor.hpp"
#include "../Common/Hex.hpp"

ContactListScreen::ContactListScreen(Root *root)
{
	_root = root;
	_contacts = root->Messages->GetContactStorage();

	_myName = _root->Conf->GetName() + "@" + _root->Conf->GetHostName();

	_receivedContactList = false;
	_contactRequestSuccess = false;

	_currentContact = 0;

	_mode = Mode::List;
	_manageMode = ManageMode::AddContact;

	RequestContactList();
}

ContactListScreen::~ContactListScreen()
{
	_root->Network->SetContactListProcessor(nullptr);
}

void ContactListScreen::Redraw()
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
		_root->Conf->GetHostName() + " server contact list",
		COLOR_PAIR(YELLOW_TEXT));

	RedrawContactList();

	if (_mode == Mode::Manage) {
		DrawManageWindow();
	}
}

Screen *ContactListScreen::ProcessEvent(int event)
{
	if (_mode == Mode::List) {
		return ProcessListEvent(event);
	} else if (_mode == Mode::Manage) {
		return ProcessManageEvent(event);
	}

	return this;
}

CowBuffer<String> ContactListScreen::GetControlHelp()
{
	if (_mode == Mode::List) {
		CowBuffer<String> result(3);
		result[0] = "Back: " + _root->Conf->ContactBackName();
		result[1] = "Up/Down: " + _root->Conf->ContactUpName() + "/" +
			_root->Conf->ContactDownName();
		result[2] = "Select: " + _root->Conf->ContactEnterName();

		return result;
	} else if (_mode == Mode::Manage) {
		CowBuffer<String> result(3);
		result[0] = "Back: " + _root->Conf->ContactBackName();
		result[1] = "Up/Down: " + _root->Conf->ContactUpName() + "/" +
			_root->Conf->ContactDownName();
		result[2] = "Select: " + _root->Conf->ContactEnterName();

		return result;
	}

	THROW("Must never be reached.");
}

void ContactListScreen::ProcessContactList(
	bool success,
	const CommandListContacts::Response &contactList)
{
	_receivedContactList = true;

	if (!success) {
		_contactRequestSuccess = false;
		_root->Ui->Redraw();
		return;
	}

	_contactRequestSuccess = true;
	_contactList = contactList;
	_root->Ui->Redraw();
}

void ContactListScreen::RedrawContactList()
{
	if (!_receivedContactList) {
		DrawNotification("Please wait", "Waiting for contact list.");
		return;
	}

	if (!_contactRequestSuccess) {
		DrawNotification("Error", "Failed to download contact list.");
		return;
	}

	int size = _rows - 9;

	if (!_contactList.Data.Size()) {
		move(6, 3);
		return;
	}

	int lowIndex = _currentContact;
	int highIndex = _currentContact;

	while (highIndex - lowIndex + 1 < size) {
		bool growSuccess = false;

		if (lowIndex > 0) {
			--lowIndex;
			growSuccess = true;
		}

		if (highIndex - lowIndex + 1 >= size) {
			break;
		}

		if (highIndex < (int)_contactList.Data.Size() - 1) {
			++highIndex;
			growSuccess = true;
		}

		if (!growSuccess) {
			break;
		}
	}

	int currentContactPosition = 6;

	for (int i = lowIndex, index = 0; i <= highIndex; i++, index++) {
		move(index + 6, 3);

		if (i == _currentContact) {
			attrset(COLOR_PAIR(YELLOW_TEXT));
			currentContactPosition = index + 6;
		}

		addstr(_contactList.Data[i].Name.CStr());

		if (i == _currentContact) {
			attrset(COLOR_PAIR(DEFAULT_TEXT));
		}

		if (_myName == _contactList.Data[i].Name) {
			attrset(COLOR_PAIR(GREEN_TEXT));
			addstr(" my account");
			attrset(COLOR_PAIR(DEFAULT_TEXT));
			continue;
		}

		bool alreadyInContacts = _contacts->HasContact(
			_contactList.Data[i].Name);

		if (alreadyInContacts) {
			attrset(COLOR_PAIR(GREEN_TEXT));
			addstr(" in contacts");
			attrset(COLOR_PAIR(DEFAULT_TEXT));

			Contact *contact = _contacts->GetContact(
				_contactList.Data[i].Name);

			if (!contact->IsKeyVerified(_contactList.Data[i].Key)) {
				addstr(", unverified key");
			}

			if (contact->GetBlockStatus() !=
				Contact::BlockStatus::Allowed)
			{
				addstr(", ");
				attrset(COLOR_PAIR(RED_TEXT));
				addstr("blocked");
				attrset(COLOR_PAIR(DEFAULT_TEXT));
			}
		} else {
			attrset(COLOR_PAIR(YELLOW_TEXT));
			addstr(" not in contacts");
			attrset(COLOR_PAIR(DEFAULT_TEXT));
		}
	}

	move(currentContactPosition, 2);
}

void ContactListScreen::DrawNotification(String caption, String message)
{
	int baseY = _rows / 2;
	int baseX = _columns / 2;

	int offsetX = message.Length() / 2 + 1;

	UiHelpers::DrawFrame(
		baseY - 2,
		baseY + 2,
		baseX - offsetX - 1,
		baseX + offsetX + 1,
		caption,
		COLOR_PAIR(YELLOW_TEXT));

	move(baseY, baseX - offsetX + 1);
	addstr(message.CStr());
}

Screen *ContactListScreen::ProcessListEvent(int event)
{
	if (event == _root->Conf->ContactBackKey()) {
		return nullptr;
	}

	if (event == _root->Conf->ContactUpKey()) {
		--_currentContact;

		if (_currentContact < 0) {
			++_currentContact;
		}

		return this;
	}

	if (event == _root->Conf->ContactDownKey()) {
		++_currentContact;

		if (_currentContact >= (int)_contactList.Data.Size()) {
			--_currentContact;
		}

		return this;
	}

	if (event == _root->Conf->ContactEnterKey()) {
		if (!_contactList.Data.Size()) {
			_root->Ui->Notify("No contacts. Nothing to do.");
			return this;
		}

		if (_contactList.Data[_currentContact].Name == _myName) {
			_root->Ui->Notify("It is your account.");
			return this;
		}

		_mode = Mode::Manage;
		_manageMode = ManageMode::AddContact;
		return this;
	}

	return this;
}

Screen *ContactListScreen::ProcessManageEvent(int event)
{
	if (event == _root->Conf->ContactBackKey()) {
		_mode = Mode::List;
		_manageMode = ManageMode::AddContact;
		return this;
	}

	String currentContactName = _contactList.Data[_currentContact].Name;
	Crypto::X25519::PublicKeyContainer currentContactKey =
		_contactList.Data[_currentContact].Key;

	if (event == _root->Conf->ContactUpKey()) {
		if (_manageMode == ManageMode::ValidateKey) {
			_manageMode = ManageMode::AddContact;
		}

		return this;
	}

	if (event == _root->Conf->ContactDownKey()) {
		if (_manageMode == ManageMode::AddContact) {
			if (_contacts->HasContact(currentContactName)) {
				_manageMode = ManageMode::ValidateKey;
			}
		}

		return this;
	}

	if (event == _root->Conf->ContactEnterKey()) {
		if (_manageMode == ManageMode::AddContact) {
			if (_contacts->HasContact(currentContactName)) {
				_root->Ui->Notify(
					"Contact is already in "
					"your contact storage.");
			} else {
				bool success = _root->Network->AddContact(
					currentContactName);

				if (!success) {
					_root->Ui->Notify(
						"Failed to add new contact. "
						"No connection.");
				}
			}
		} else if (_manageMode == ManageMode::ValidateKey) {
			Contact *contact = _contacts->GetContact(
				currentContactName);

			if (!contact) {
				_manageMode = ManageMode::AddContact;
				return this;
			}

			if (contact->IsKeyVerified(currentContactKey)) {
				_root->Ui->Notify("Key is already verified.");
			} else {
				bool success = _root->Network->UpdateContactKey(
					currentContactName,
					currentContactKey,
					true,
					false,
					false);

				if (!success) {
					_root->Ui->Notify(
						"Failed to add new contact "
						"key. No connection.");
				}
			}
		}

		return this;
	}

	return this;
}

void ContactListScreen::DrawManageWindow()
{
	int baseY = _rows / 2;
	int baseX = _columns / 2;

	UiHelpers::ClearScreen(
		baseY - 4,
		baseY + 5,
		0,
		_columns - 1);

	UiHelpers::DrawFrame(
		baseY - 3,
		baseY + 4,
		1,
		_columns - 2,
		"Manage contact " + _contactList.Data[_currentContact].Name,
		COLOR_PAIR(YELLOW_TEXT));

	String currentContactName = _contactList.Data[_currentContact].Name;
	Crypto::X25519::PublicKeyContainer currentContactKey =
		_contactList.Data[_currentContact].Key;

	move(baseY - 1, baseX - 10);

	if (!_contacts->HasContact(currentContactName)) {
		addstr("Add to contact list");
		_manageMode = ManageMode::AddContact;
	} else {
		addstr("Already in contact list");

		move(baseY + 1, baseX - 34);
		addstr("Key: ");
		addstr(DataToHex(
			currentContactKey.Key,
			Crypto::X25519::KEY_SIZE).CStr());

		move(baseY + 2, baseX - 10);
		Contact *contact = _contacts->GetContact(currentContactName);

		if (contact->IsKeyVerified(currentContactKey)) {
			addstr("Key is verified");
		} else {
			addstr("Add key to verified");
		}
	}

	if (_manageMode == ManageMode::AddContact) {
		move(baseY - 1, baseX - 11);
	} else if (_manageMode == ManageMode::ValidateKey) {
		move(baseY + 2, baseX - 11);
	}
}

void ContactListScreen::RequestContactList()
{
	bool success = _root->Network->SetContactListProcessor(this);

	if (!success) {
		_contactRequestSuccess = false;
		_receivedContactList = true;
		_root->Ui->Notify("Failed to request contact list.");
	}

	success = _root->Network->ListContacts();

	if (!success) {
		_contactRequestSuccess = false;
		_receivedContactList = true;
		_root->Ui->Notify("Failed to request contact list.");
	}
}
