#include "WorkScreen.hpp"

#include <curses.h>

#include "LoginScreen.hpp"
#include "ContactScreen.hpp"
//#include "AttachmentScreen.hpp"
#include "TextColor.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/Hex.hpp"
#include "../Common/File.hpp"

WorkScreen::WorkScreen(Root *root)
{
	_root = root;
}

WorkScreen::~WorkScreen()
{ }

void WorkScreen::Redraw()
{
	RedrawFrames();
	RedrawContactList();
	RedrawCurrentChat();
	RedrawTextBox();
}

Screen *WorkScreen::ProcessEvent(int event)
{
	if (event == _root->Conf->WorkConnectKey()) {
		return new LoginScreen(_root);
	}

	if (event == _root->Conf->WorkContactKey()) {
		return new ContactScreen(_root);
	}

	if (event == _root->Conf->WorkExitKey()) {
		return nullptr;
	}

	return this;
}

CowBuffer<String> WorkScreen::GetControlHelp()
{
	CowBuffer<String> result(3);
	result[0] = "Exit: " + _root->Conf->WorkExitName();
	result[1] = "Manage contacts: " + _root->Conf->WorkContactName();
	result[2] = "Connect: " + _root->Conf->WorkConnectName();

	return result;
}

void WorkScreen::RedrawFrames()
{
	/*  Login
	 *  PublicKey
	 *  ConnectionStatus
	 *  VoiceStatus
	 *  ----------------------------------
	 *  CurrentContactName
	 *  ----------------------------------
	 *  ContactList | CurrentChatMessages
	 *              |
	 *              |
	 *              |
	 *              |
	 *              |
	 *              |---------------------
	 *              | CurrentChatTextbox
	 *              |
	 *  ----------------------------------
	 *  Help
	 */

	const int h1Y = 4;
	const int h2Y = 6;
	const int h3Y = _rows - 9;
	const int h4Y = _rows - 3;

	const int v1X = _columns / 4;

	for (int i = 0; i < _columns; i++) {
		move(h1Y, i);
		addch(ACS_HLINE);

		move(h2Y, i);
		addch(ACS_HLINE);

		if (i > v1X) {
			move(h3Y, i);
			addch(ACS_HLINE);
		}

		move(h4Y, i);
		addch(ACS_HLINE);
	}

	for (int i = h2Y + 1; i < h4Y; i++) {
		move(i, v1X);
		addch(ACS_VLINE);
	}

	move(h2Y, v1X);
	addch(ACS_TTEE);

	move(h4Y, v1X);
	addch(ACS_BTEE);

	move(h3Y, v1X);
	addch(ACS_LTEE);

	move(4, 0);
}

void WorkScreen::RedrawContactList()
{
}

void WorkScreen::RedrawCurrentChat()
{
}

void WorkScreen::RedrawTextBox()
{
}
