#include "ContactManageScreen.hpp"

#include <curses.h>

#include "WorkScreen.hpp"
#include "TextColor.hpp"
#include "../Common/Hex.hpp"

ContactManageScreen::ContactManageScreen(Root *root, String contactName)
{
	_root = root;
	_contacts = root->Messages->GetContactStorage();
	_contactName = contactName;

	_currentKey = -1;

	_mode = Mode::List;
	_manageMode = ManageMode::Validation;

	_newKeyHex.Caption = "New key: ";
}

ContactManageScreen::~ContactManageScreen()
{
}

void ContactManageScreen::Redraw()
{
	for (int i = 0; i < _columns; i++) {
		move(4, i);
		addch(ACS_HLINE);
	}

	bool running;
	UiHelpers::DrawFrame(
		5,
		_rows - 3,
		1,
		_columns - 2,
		"Manage " + _contactName,
		COLOR_PAIR(YELLOW_TEXT),
		running);

	if (running) {
		_root->Ui->RequestRunningLine();
	}

	Contact *contact = _contacts->GetContact(_contactName);

	if (!contact) {
		move(6, 3);
		running = UiHelpers::DrawRunningLine(
			"Contact is deleted.",
			_columns - 8);

		if (running) {
			_root->Ui->RequestRunningLine();
		}

		return;
	}

	RedrawKeyList();

	if (_mode == Mode::Add) {
		DrawAddWindow();
	} else if (_mode == Mode::Manage) {
		DrawManageKeyWindow();
	}
}

Screen *ContactManageScreen::ProcessEvent(int event)
{
	if (!_contacts->GetContact(_contactName)) {
		return nullptr;
	}

	if (_mode == Mode::List) {
		return ProcessListEvent(event);
	} else if (_mode == Mode::Add) {
		return ProcessAddEvent(event);
	} else if (_mode == Mode::Manage) {
		return ProcessManageEvent(event);
	}

	return this;
}

CowBuffer<String> ContactManageScreen::GetControlHelp()
{
	if (_mode == Mode::List) {
		CowBuffer<String> result(4);
		result[0] = "Back: " + _root->Conf->ContactBackName();
		result[1] = "Up/Down: " + _root->Conf->ContactUpName() + "/" +
			_root->Conf->ContactDownName();
		result[2] = "Select: " + _root->Conf->ContactEnterName();
		result[3] = "New: " + _root->Conf->ContactNewName();

		return result;
	} else if (_mode == Mode::Add) {
		CowBuffer<String> result(2);
		result[0] = "Back: " + _root->Conf->ContactBackName();
		result[1] = "Create key: " +
			_root->Conf->ContactEnterName();

		return result;
	} else if (_mode == Mode::Manage) {
		CowBuffer<String> result(3);
		result[0] = "Back: " + _root->Conf->ContactBackName();
		result[1] = "Up/Down: " + _root->Conf->ContactUpName() + "/" +
			_root->Conf->ContactDownName();
		result[2] = "Change: " + _root->Conf->ContactEnterName();

		return result;
	}

	THROW("Must never be reached.");
}

void ContactManageScreen::RedrawKeyList()
{
	int size = _rows - 9;

	Contact *contact = _contacts->GetContact(_contactName);

	CowBuffer<Crypto::X25519::PublicKeyContainer> keys =
		contact->GetKeys();

	if (!keys.Size()) {
		move(6, 2);
		return;
	}

	if (_currentKey >= (int)keys.Size()) {
		_currentKey = keys.Size() - 1;
	}

	if (_currentKey < 0) {
		_currentKey = 0;
	}

	int firstKey = _currentKey;
	int lastKey = _currentKey;

	while (lastKey - firstKey + 1 < size) {
		bool moved = false;

		if (firstKey) {
			--firstKey;
			moved = true;
		}

		if (lastKey - firstKey + 1 >= size) {
			break;
		}

		if (lastKey < (int)keys.Size() - 1) {
			++lastKey;
			moved = true;
		}

		if (!moved) {
			break;
		}
	}

	int currentKeyPosition = 6;
	int selectedKeyPosition = 6;

	for (int i = firstKey; i <= lastKey; i++) {
		move(currentKeyPosition, 3);

		String keyString = DataToHex(
			keys[i].Key,
			Crypto::X25519::KEY_SIZE);

		String statusString;

		if (contact->IsKeyBlocked(keys[i])) {
			statusString += "blocked";
		}

		if (!contact->IsKeyVerified(keys[i])) {
			if (statusString.Length()) {
				statusString += ", ";
			}

			statusString += "unverified";
		}

		if (contact->HasDefaultKey()) {
			if (contact->GetDefaultKey() == keys[i]) {
				if (statusString.Length()) {
					statusString += ", ";
				}

				statusString += "default";
			}
		}

		int keyLength = keyString.Length();
		int statusLength = statusString.Length();

		if (statusLength) {
			keyLength += 3;
		}

		int widthLimit = _columns - 7;

		if (keyLength + statusLength > widthLimit) {
			if (statusLength > widthLimit / 4) {
				statusLength = widthLimit / 4;
			}

			keyLength = widthLimit - statusLength;

			if (statusLength) {
				keyLength -= 3;
			}
		}

		if (i == _currentKey) {
			attrset(COLOR_PAIR(YELLOW_TEXT));
			selectedKeyPosition = currentKeyPosition;
		}

		bool running = UiHelpers::DrawRunningLine(keyString, keyLength);

		if (running) {
			_root->Ui->RequestRunningLine();
		}

		if (i == _currentKey) {
			attrset(COLOR_PAIR(DEFAULT_TEXT));
		}

		if (statusString.Length()) {
			addstr(" | ");

			if (contact->IsKeyBlocked(keys[i])) {
				attrset(COLOR_PAIR(RED_TEXT));
			} else if (!contact->IsKeyVerified(keys[i])) {
				attrset(COLOR_PAIR(YELLOW_TEXT));
			}

			running = UiHelpers::DrawRunningLine(
				statusString,
				statusLength);

			if (running) {
				_root->Ui->RequestRunningLine();
			}

			attrset(COLOR_PAIR(DEFAULT_TEXT));
		}

		++currentKeyPosition;
	}

	move(selectedKeyPosition, 2);
}

Screen *ContactManageScreen::ProcessListEvent(int event)
{
	if (event == _root->Conf->ContactBackKey()) {
		return nullptr;
	}

	if (event == _root->Conf->ContactUpKey()) {
		--_currentKey;
		return this;
	}

	if (event == _root->Conf->ContactDownKey()) {
		++_currentKey;
		return this;
	}

	if (event == _root->Conf->ContactEnterKey()) {
		if (_currentKey < 0) {
			_root->Ui->Notify("No keys. Nothing to do.");
			return this;
		}

		_mode = Mode::Manage;
		_manageMode = ManageMode::Validation;
		return this;
	}

	if (event == _root->Conf->ContactNewKey()) {
		_mode = Mode::Add;
		return this;
	}

	return this;
}

void ContactManageScreen::DrawAddWindow()
{
	int baseY = _rows / 2;
	int baseX = _columns / 2;

	int xOffset =
		(_newKeyHex.Caption.Length() +
		_newKeyHex.Text.Length()) /
		2;

	if (xOffset > _columns / 2 - 4) {
		xOffset = _columns / 2 - 4;
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
		"New key",
		COLOR_PAIR(YELLOW_TEXT),
		running);

	if (running) {
		_root->Ui->RequestRunningLine();
	}

	_newKeyHex.SetCaptionPosition(baseY, baseX - xOffset);
	_newKeyHex.SetWidthLimit(xOffset * 2 + 1);
	_newKeyHex.AlignTextToCaption();
	_newKeyHex.Redraw();
}

static bool ValidChar(int event)
{
	return (event >= '0' && event <= '9') ||
		(event >= 'a' && event <= 'f') ||
		event == '\b';
}

Screen *ContactManageScreen::ProcessAddEvent(int event)
{
	if (event == _root->Conf->ContactBackKey()) {
		_mode = Mode::List;
		return this;
	}

	if (event == _root->Conf->ContactEnterKey()) {
		if (_newKeyHex.Text.Length() != Crypto::X25519::KEY_SIZE * 2) {
			_root->Ui->Notify("Key length must be 64 characters.");
			return this;
		}

		Crypto::X25519::PublicKeyContainer key;
		HexToData(_newKeyHex.Text, key.Key);

		CowBuffer<Crypto::X25519::PublicKeyContainer> keys =
			_contacts->GetContact(_contactName)->GetKeys();

		for (unsigned int i = 0 ; i < keys.Size(); i++) {
			if (key == keys[i]) {
				_root->Ui->Notify("Key already exists.");
				_currentKey = i;
				return this;
			}
		}

		bool requestSuccess = _root->Network->UpdateContactKey(
			_contactName,
			key,
			false,
			false,
			false);

		if (!requestSuccess) {
			_root->Ui->Notify(
				"Failed to add new key. No connection.");
			return this;
		}

		_newKeyHex.Text = "";
		_mode = Mode::List;
		_currentKey = keys.Size();
		return this;
	}

	if (ValidChar(event)) {
		if (event != '\b' && _newKeyHex.Text.Length() >= 64) {
			_root->Ui->Notify("Key length limit reached.");
			return this;
		}

		_newKeyHex.ProcessChar(event);
	} else {
		_root->Ui->Notify("Invalid character.");
	}

	return this;
}

void ContactManageScreen::DrawManageKeyWindow()
{
	int baseY = _rows / 2;
	int baseX = _columns / 2;

	UiHelpers::ClearScreen(
		baseY - 4,
		baseY + 4,
		0,
		_columns - 1);

	Contact *contact = _contacts->GetContact(_contactName);

	CowBuffer<Crypto::X25519::PublicKeyContainer> keys =
		contact->GetKeys();

	String keyHex = DataToHex(
		keys[_currentKey].Key,
		Crypto::X25519::KEY_SIZE);

	bool running;
	UiHelpers::DrawFrame(
		baseY - 3,
		baseY + 3,
		1,
		_columns - 2,
		"Manage " + keyHex,
		COLOR_PAIR(YELLOW_TEXT),
		running);

	if (running) {
		_root->Ui->RequestRunningLine();
	}

	bool validated = contact->IsKeyVerified(keys[_currentKey]);
	bool blocked = contact->IsKeyBlocked(keys[_currentKey]);

	bool defaultKey = false;

	if (contact->HasDefaultKey()) {
		if (keys[_currentKey] == contact->GetDefaultKey()) {
			defaultKey = true;
		}
	}

	move(baseY - 1, baseX - 5);
	addstr("Validated: ");
	addstr(validated ? "yes" : "no");

	move(baseY, baseX - 5);
	addstr("Blocked: ");
	addstr(blocked ? "yes" : "no");

	move(baseY + 1, baseX - 5);
	addstr("Default: ");
	addstr(defaultKey ? "yes" : "no");

	if (_manageMode == ManageMode::Validation) {
		move(baseY - 1, baseX - 6);
	} else if (_manageMode == ManageMode::Block) {
		move(baseY, baseX - 6);
	} else if (_manageMode == ManageMode::Default) {
		move(baseY + 1, baseX - 6);
	}
}

Screen *ContactManageScreen::ProcessManageEvent(int event)
{
	if (event == _root->Conf->ContactBackKey()) {
		_mode = Mode::List;
		return this;
	}

	if (event == _root->Conf->ContactDownKey()) {
		if (_manageMode == ManageMode::Validation) {
			_manageMode = ManageMode::Block;
		} else if (_manageMode == ManageMode::Block) {
			_manageMode = ManageMode::Default;
		}

		return this;
	}

	if (event == _root->Conf->ContactUpKey()) {
		if (_manageMode == ManageMode::Block) {
			_manageMode = ManageMode::Validation;
		} else if (_manageMode == ManageMode::Default) {
			_manageMode = ManageMode::Block;
		}

		return this;
	}

	if (event == _root->Conf->ContactEnterKey()) {
		Contact *contact = _contacts->GetContact(_contactName);

		CowBuffer<Crypto::X25519::PublicKeyContainer> keys =
			contact->GetKeys();

		bool validated = contact->IsKeyVerified(keys[_currentKey]);
		bool blocked = contact->IsKeyBlocked(keys[_currentKey]);

		bool defaultKey = false;

		if (contact->HasDefaultKey()) {
			if (keys[_currentKey] == contact->GetDefaultKey()) {
				defaultKey = true;
			}
		}

		if (_manageMode == ManageMode::Validation) {
			validated = !validated;
		} else if (_manageMode == ManageMode::Block) {
			blocked = !blocked;
		} else if (_manageMode == ManageMode::Default) {
			if (defaultKey) {
				_root->Ui->Notify(
					"Default status can not be unset "
					"directly. "
					"Select another key as default.");
				return this;
			}

			defaultKey = true;
		}

		bool requestSuccess = _root->Network->UpdateContactKey(
			_contactName,
			keys[_currentKey],
			validated,
			blocked,
			defaultKey);

		if (!requestSuccess) {
			_root->Ui->Notify(
				"Failed to update key properties. "
				"No connection.");
		}

		return this;
	}

	return this;
}
