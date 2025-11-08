#ifndef _UI_HPP
#define _UI_HPP

#include <curses.h>

#include "../Protocol/Protocol.hpp"

class UI
{
public:
	UI(ClientSession *session);
	~UI();

	bool ProcessEvent();

private:
	enum UIState
	{
		UIStatePassword = 0,
		UIStateConnect = 1,
		UIStateWork = 2
	};

	UIState _state;

	int _rows;
	int _columns;

	void Redraw();
	void RedrawPassword();
	void RedrawConnect();
	void RedrawWork();

	void ProcessResize();

	void ProcessPassword(int event);
	void ProcessConnect(int event);
	void ProcessWork(int event);

	ClientSession *_session;

	String _password;

	bool _enterIp;
	String _ip;
	String _port;
};

#endif
