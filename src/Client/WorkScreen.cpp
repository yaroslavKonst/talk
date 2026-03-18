#include "WorkScreen.hpp"

#include <curses.h>

#include "LoginScreen.hpp"
//#include "AttachmentScreen.hpp"
#include "TextColor.hpp"
#include "../Protocol/ActiveSession.hpp"
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
	RedrawHelp();
}

Screen *WorkScreen::ProcessEvent(int event)
{
	if (event == _root->Conf->WorkConnectKey()) {
		return new LoginScreen(_root);
	}

	if (event == _root->Conf->WorkExitKey()) {
		return nullptr;
	}

	return this;
}

void WorkScreen::RedrawFrames()
{
	/*  ConnectionStatus
	 *  VoiceStatus
	 *  ----------------------------------
	 *  CurrentContactName
	 *  CurrentContactKey
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

	const int h1Y = 2;
	const int h2Y = 5;
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

void WorkScreen::RedrawHelp()
{
}
