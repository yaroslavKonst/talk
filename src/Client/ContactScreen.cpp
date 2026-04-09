#include "ContactScreen.hpp"

#include <curses.h>

#include "WorkScreen.hpp"
#include "TextColor.hpp"

ContactScreen::ContactScreen(Root *root)
{
	_root = root;
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

	move(5, 0);
	addstr("Contacts");

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

void ContactScreen::RedrawContactList()
{
	int size = _rows - 8;

	CowBuffer<String> names = _contacts->GetContactRange(
		_currentContact,
		size);

	int currentContactPosition = 7;

	for (unsigned int i = 0; i < names.Size(); i++) {
		move(i + 7, 4);

		if (names[i] == _currentContact) {
			attrset(COLOR_PAIR(YELLOW_TEXT));
			currentContactPosition = i + 7;
		}

		addstr(names[i].CStr());

		if (names[i] == _currentContact) {
			attrset(COLOR_PAIR(DEFAULT_TEXT));
		}
	}

	move(currentContactPosition, 2);
}

Screen *ContactScreen::ProcessListEvent(int event)
{
	if (event == _root->Conf->ContactBackKey()) {
		return new WorkScreen(_root);
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

	if (event == _root->Conf->ContactNewKey()) {
		_mode = Mode::Add;
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

		_contacts->AddNewContact(_newContactName.Text);
		_currentContact = _newContactName.Text;
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
