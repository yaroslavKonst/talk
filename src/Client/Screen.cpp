#include "Screen.hpp"

#include <curses.h>

Screen::Screen()
{
	getmaxyx(stdscr, _rows, _columns);
}

Screen::~Screen()
{
}

void Screen::ProcessResize()
{
	getmaxyx(stdscr, _rows, _columns);
	this->ProcessResizeScreen();
}
