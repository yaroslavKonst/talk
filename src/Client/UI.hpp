#ifndef _UI_HPP
#define _UI_HPP

#include <curses.h>

#include "../Protocol/Protocol.hpp"

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

class PasswordScreen : public Screen
{
public:
	PasswordScreen(ClientSession *session);

	void Redraw() override;
	Screen *ProcessEvent(int event) override;

private:
	String _password;

	void GenerateKeys();
};

class LoginScreen : public Screen
{
public:
	LoginScreen(ClientSession *session);

	void Redraw() override;
	Screen *ProcessEvent(int event) override;

private:
	bool _writingIp;
	bool _writingPort;
	bool _writingKey;

	String _ip;
	String _port;
	String _serverKeyHex;
};

class WorkScreen : public Screen
{
public:
	WorkScreen(ClientSession *session);

	void Redraw() override;
	Screen *ProcessEvent(int event) override;

private:
};

class UI
{
public:
	UI(ClientSession *session);
	~UI();

	bool ProcessEvent();

private:
	Screen *_screen;

	void ProcessResize();

	ClientSession *_session;
};

#endif
