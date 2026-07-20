#ifndef _SCREEN_HPP
#define _SCREEN_HPP

#include "../Common/MyString.hpp"

class Screen
{
public:
	Screen();
	virtual ~Screen();

	virtual Screen *ProcessEvent(int event) = 0;
	virtual void Redraw() = 0;
	void ProcessResize();

	virtual CowBuffer<String> GetControlHelp() = 0;

protected:
	int _rows;
	int _columns;
};

#endif
