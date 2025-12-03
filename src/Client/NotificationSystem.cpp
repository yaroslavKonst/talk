#include "NotificationSystem.hpp"

#include <curses.h>

#include "TextColor.hpp"

NotificationSystem::NotificationSystem(
	NotifyRedrawHandler *handler,
	ControlStorage *controls)
{
	_first = nullptr;
	_last = nullptr;

	_handler = handler;
	_controls = controls;
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

	_handler->NotifyRedraw();
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
	int frameSize = messageSize + 2;

	if (frameSize < 30) {
		frameSize = 30;
	}

	int baseY = rows / 2 - 4;
	int limitY = rows / 2 + 4;

	int baseX = columns / 2 - frameSize / 2 - 2;
	int limitX = columns / 2 + frameSize / 2 + 3;

	// Clear.
	for (int r = baseY; r < limitY; r++) {
		for (int c = baseX; c < limitX; c++) {
			move(r, c);
			addch(' ');
		}
	}

	// Frame.
	attrset(COLOR_PAIR(YELLOW_TEXT));

	for (int r = baseY + 2; r < limitY - 2; r++) {
		move(r, baseX + 1);
		addch(ACS_VLINE);

		move(r, limitX - 2);
		addch(ACS_VLINE);
	}

	for (int c = baseX + 2; c < limitX - 2; c++) {
		move(baseY + 1, c);
		addch(ACS_HLINE);

		move(limitY - 2, c);
		addch(ACS_HLINE);
	}

	move(baseY + 1, baseX + 1);
	addch(ACS_ULCORNER);
	move(baseY + 1, limitX - 2);
	addch(ACS_URCORNER);
	move(limitY - 2, baseX + 1);
	addch(ACS_LLCORNER);
	move(limitY - 2, limitX - 2);
	addch(ACS_LRCORNER);

	move(baseY + 1, baseX + 2);
	addstr("Notification");

	attrset(COLOR_PAIR(DEFAULT_TEXT));

	// Message.
	move(baseY + 3, columns / 2 - messageSize / 2);
	addstr(_first->Message.CStr());

	move(baseY + 5, columns / 2 - 10);
	addstr(("Press " +
		_controls->NotificationConfirmName() +
		" to close.").CStr());
}

bool NotificationSystem::ProcessEvent(int event)
{
	if (!_first) {
		return false;
	}

	if (event == _controls->NotificationConfirmKey()) {
		Notification *tmp = _first;
		_first = _first->Next;
		delete tmp;

		if (!_first) {
			_last = nullptr;
		}
	}

	return true;
}
