#include "NotificationSystem.hpp"

#include <curses.h>

#include "TextColor.hpp"
#include "UiHelpers.hpp"

NotificationSystem::NotificationSystem(Root *root)
{
	_first = nullptr;
	_last = nullptr;

	_root = root;
}

NotificationSystem::~NotificationSystem()
{
	while (_first) {
		Notification *tmp = _first;
		_first = _first->Next;
		delete tmp;
	}

	_last = nullptr;
}

void NotificationSystem::Notify(String message)
{
	Notification *notification = new Notification;
	notification->Next = nullptr;
	notification->Message = message;

	if (!_first) {
		_first = notification;
		_last = notification;
	} else {
		_last->Next = notification;
		_last = notification;
	}

	_root->Ui->Redraw();
}

void NotificationSystem::Redraw()
{
	if (!_first) {
		return;
	}

	int rows;
	int columns;
	getmaxyx(stdscr, rows, columns);

	int messageSize = _first->Message.Length();
	int frameSize = messageSize + 3;

	if (frameSize < 30) {
		frameSize = 30;
	}

	int baseY = rows / 2 - 4;
	int limitY = rows / 2 + 3;

	int baseX = columns / 2 - frameSize / 2 - 2;
	int limitX = columns / 2 + frameSize / 2 + 2;

	UiHelpers::ClearScreen(baseY, limitY, baseX, limitX);

	UiHelpers::DrawFrame(
		baseY + 1,
		limitY - 1,
		baseX + 1,
		limitX - 1,
		"Notification",
		COLOR_PAIR(YELLOW_TEXT));

	// Message.
	move(baseY + 3, columns / 2 - messageSize / 2);
	addstr(_first->Message.CStr());

	move(baseY + 5, columns / 2 - 10);
	addstr(("Press " +
		_root->Conf->NotificationConfirmName() +
		" to close.").CStr());
}

bool NotificationSystem::ProcessEvent(int event)
{
	if (!_first) {
		return false;
	}

	if (event == _root->Conf->NotificationConfirmKey()) {
		Notification *tmp = _first;
		_first = _first->Next;
		delete tmp;

		if (!_first) {
			_last = nullptr;
		}
	}

	return true;
}
