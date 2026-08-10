#include "NotificationSystem.hpp"

#include <curses.h>

#include "TextColor.hpp"
#include "UiHelpers.hpp"

NotificationSystem::NotificationSystem(Root *root)
{
	_first = nullptr;
	_last = nullptr;

	_blockFirst = nullptr;
	_blockLast = nullptr;

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

	while (_blockFirst) {
		Notification *tmp = _blockFirst;
		_blockFirst = _blockFirst->Next;
		delete tmp;
	}

	_blockLast = nullptr;
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

void *NotificationSystem::BlockNotify(String message)
{
	Notification *notification = new Notification;
	notification->Next = nullptr;
	notification->Message = message;

	if (!_blockFirst) {
		_blockFirst = notification;
		_blockLast = notification;
	} else {
		_blockLast->Next = notification;
		_blockLast = notification;
	}

	_root->Ui->Redraw();

	return _blockLast;
}

void NotificationSystem::BlockCancel(void *handle)
{
	Notification *node = reinterpret_cast<Notification*>(handle);

	if (node == _blockFirst) {
		_blockFirst = _blockFirst->Next;

		if (!_blockFirst) {
			_blockLast = nullptr;
		}

		delete node;
		_root->Ui->Redraw();
		return;
	}

	Notification *prevNode = _blockFirst;

	while (prevNode) {
		if (prevNode->Next != node) {
			prevNode = prevNode->Next;
			continue;
		}

		prevNode->Next = node->Next;

		if (_blockLast == node) {
			_blockLast = prevNode;
		}

		delete node;
		_root->Ui->Redraw();
		return;
	}

	_root->Ui->Redraw();
}

void NotificationSystem::Redraw()
{
	Notification *node;

	if (_blockFirst) {
		node = _blockFirst;
	} else if (_first) {
		node = _first;
	} else {
		return;
	}

	int rows;
	int columns;
	getmaxyx(stdscr, rows, columns);

	int messageSize = node->Message.Length();

	if (messageSize > columns - 7) {
		messageSize = columns - 7;
	}

	int frameSize = messageSize + 3;

	if (frameSize < 30) {
		frameSize = 30;
	}

	if (frameSize > columns - 5) {
		frameSize = columns - 5;
	}

	int baseY = rows / 2 - 4;
	int limitY = rows / 2 + 3;

	int baseX = columns / 2 - frameSize / 2 - 2;
	int limitX = columns / 2 + frameSize / 2 + 2;

	UiHelpers::ClearScreen(baseY, limitY, baseX, limitX);

	bool running;
	UiHelpers::DrawFrame(
		baseY + 1,
		limitY - 1,
		baseX + 1,
		limitX - 1,
		node == _first ? "Notification" : "Please wait",
		COLOR_PAIR(YELLOW_TEXT),
		running);

	if (running) {
		_root->Ui->RequestRunningLine();
	}

	// Message.
	move(baseY + 3, columns / 2 - messageSize / 2);
	running = UiHelpers::DrawRunningLine(node->Message, frameSize - 3);

	if (running) {
		_root->Ui->RequestRunningLine();
	}

	if (node == _first) {
		move(baseY + 5, columns / 2 - 10);
		String controlLine = "Press " +
			_root->Conf->NotificationConfirmName() +
			" to close.";

		running = UiHelpers::DrawRunningLine(
			controlLine,
			frameSize - 3);

		if (running) {
			_root->Ui->RequestRunningLine();
		}
	} else {
		move(baseY + 5, columns / 2);
	}
}

bool NotificationSystem::ProcessEvent(int event)
{
	if (_blockFirst) {
		return true;
	}

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
