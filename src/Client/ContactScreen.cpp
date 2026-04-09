#include "ContactScreen.hpp"

#include <curses.h>

#include "WorkScreen.hpp"
#include "TextColor.hpp"

ContactScreen::ContactScreen(Root *root)
{
	_root = root;
	_contacts = root->Messages->GetContactStorage();
	_currentContact = _contacts->GetFirstContact();
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
}

Screen *ContactScreen::ProcessEvent(int event)
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

	move(currentContactPosition, 3);
}
