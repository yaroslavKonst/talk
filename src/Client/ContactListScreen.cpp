#include "ContactListScreen.hpp"

#include <curses.h>

#include "WorkScreen.hpp"
#include "TextColor.hpp"
#include "UiLayout.hpp"
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
		move(LayoutConstants::HeaderHeight, i);
		addch(ACS_HLINE);
	}

	bool running;
	UiHelpers::DrawFrame(
		LayoutConstants::HeaderHeight + 1,
		_rows - 1 - LayoutConstants::FooterHeight,
		1,
		_columns - 2,
		_root->Conf->GetHostName() + " server contact list",
		COLOR_PAIR(YELLOW_TEXT),
		running);

	if (running) {
		_root->Ui->RequestRunningLine();
	}

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

	int size = _rows -
		LayoutConstants::HeaderHeight -
		LayoutConstants::FooterHeight - 3;

	if (!_contactList.Data.Size()) {
		move(LayoutConstants::HeaderHeight + 2, 3);
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

	int currentContactPosition = LayoutConstants::HeaderHeight + 2;

	for (int i = lowIndex, index = 0; i <= highIndex; i++, index++) {
		int posY = index + LayoutConstants::HeaderHeight + 2;

		move(posY, 3);

		String nameString;
		String statusString;

		int nameAttr = 0;
		int statusAttr = 0;

		if (i == _currentContact) {
			nameAttr = COLOR_PAIR(YELLOW_TEXT);
			currentContactPosition = posY;
		}

		nameString = _contactList.Data[i].Name;

		if (_myName == _contactList.Data[i].Name) {
			statusAttr = COLOR_PAIR(GREEN_TEXT);
			statusString = "my account";
		} else {
			bool alreadyInContacts = _contacts->HasContact(
				_contactList.Data[i].Name);

			if (alreadyInContacts) {
				statusAttr = COLOR_PAIR(GREEN_TEXT);
				statusString = "in contacts";

				Contact *contact = _contacts->GetContact(
					_contactList.Data[i].Name);

				if (!contact->IsKeyVerified(
					_contactList.Data[i].Key))
				{
					statusAttr = COLOR_PAIR(YELLOW_TEXT);
					statusString += ", unverified key";
				}

				if (contact->GetBlockStatus() !=
					Contact::BlockStatus::Allowed)
				{
					statusAttr = COLOR_PAIR(RED_TEXT);
					statusString += ", blocked";
				}
			} else {
				statusAttr = COLOR_PAIR(YELLOW_TEXT);
				statusString = "not in contacts";
			}
		}

		int widthLimit = _columns - 6;

		bool running = UiHelpers::DrawCommentedLine(
			nameString,
			statusString,
			widthLimit,
			nameAttr,
			statusAttr);

		if (running) {
			_root->Ui->RequestRunningLine();
		}
	}

	move(currentContactPosition, 2);
}

void ContactListScreen::DrawNotification(String caption, String message)
{
	int baseY = _rows / 2;
	int baseX = _columns / 2;

	int offsetX = UTF8::StrLen(message.CStr()) / 2 + 1;

	if (offsetX * 2 > _columns - 6) {
		offsetX = _columns / 2 - 3;
	}

	bool running;
	UiHelpers::DrawFrame(
		baseY - 2,
		baseY + 2,
		baseX - offsetX - 1,
		baseX + offsetX + 1,
		caption,
		COLOR_PAIR(YELLOW_TEXT),
		running);

	if (running) {
		_root->Ui->RequestRunningLine();
	}

	move(baseY, baseX - offsetX + 1);

	running = UiHelpers::DrawRunningLine(message, _columns - 8);

	if (running) {
		_root->Ui->RequestRunningLine();
	}
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

	bool running;
	UiHelpers::DrawFrame(
		baseY - 3,
		baseY + 4,
		1,
		_columns - 2,
		"Manage contact " + _contactList.Data[_currentContact].Name,
		COLOR_PAIR(YELLOW_TEXT),
		running);

	if (running) {
		_root->Ui->RequestRunningLine();
	}

	String currentContactName = _contactList.Data[_currentContact].Name;
	Crypto::X25519::PublicKeyContainer currentContactKey =
		_contactList.Data[_currentContact].Key;

	int xPos = baseX - 10;

	if (xPos < 3) {
		xPos = 3;
	}

	move(baseY - 1, xPos);

	running = false;
	int widthLimit = _columns - 6;

	if (!_contacts->HasContact(currentContactName)) {
		running |= UiHelpers::DrawRunningLine(
			"Add to contact list",
			widthLimit);
		_manageMode = ManageMode::AddContact;
	} else {
		running |= UiHelpers::DrawRunningLine(
			"Already in contact list",
			widthLimit);

		xPos = baseX - 34;

		if (xPos < 3) {
			xPos = 3;
		}

		move(baseY + 1, xPos);
		running |= UiHelpers::DrawRunningLine(
			"Key: " + DataToHex(
				currentContactKey.Key,
				Crypto::X25519::KEY_SIZE),
			widthLimit);

		xPos = baseX - 10;

		if (xPos < 3) {
			xPos = 3;
		}

		move(baseY + 2, xPos);
		Contact *contact = _contacts->GetContact(currentContactName);

		if (contact->IsKeyVerified(currentContactKey)) {
			running |= UiHelpers::DrawRunningLine(
				"Key is verified",
				widthLimit);
		} else {
			running |= UiHelpers::DrawRunningLine(
				"Add key to verified",
				widthLimit);
		}
	}

	xPos = baseX - 11;

	if (xPos < 2) {
		xPos = 2;
	}

	if (_manageMode == ManageMode::AddContact) {
		move(baseY - 1, xPos);
	} else if (_manageMode == ManageMode::ValidateKey) {
		move(baseY + 2, xPos);
	}

	if (running) {
		_root->Ui->RequestRunningLine();
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
