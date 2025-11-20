#include "Screen.hpp"

#include <curses.h>

Screen::Screen(ClientSession *session)
{
	_session = session;
	getmaxyx(stdscr, _rows, _columns);
}

Screen::~Screen()
{
}

void Screen::ProcessResize()
{
	getmaxyx(stdscr, _rows, _columns);
	Redraw();
}

void Screen::ClearScreen()
{
	for (int r = 0; r < _rows; r++) {
		for (int c = 0; c < _columns; c++) {
			move(r, c);
			addch(' ');
		}
	}
}
