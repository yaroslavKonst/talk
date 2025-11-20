#ifndef _SCREEN_HPP
#define _SCREEN_HPP

#include "../Protocol/ClientSession.hpp"

class Screen
{
public:
	Screen(ClientSession *session);
	virtual ~Screen();

	virtual Screen *ProcessEvent(int event) = 0;
	virtual void Redraw() = 0;
	void ProcessResize();

protected:
	int _rows;
	int _columns;

	ClientSession *_session;

	void ClearScreen();
};

#endif
