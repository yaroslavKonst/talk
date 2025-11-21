#ifndef _LOGIN_SCREEN_HPP
#define _LOGIN_SCREEN_HPP

#include "Screen.hpp"
#include "../Common/IniFile.hpp"

class LoginScreen : public Screen
{
public:
	LoginScreen(ClientSession *session, IniFile *configFile);

	void Redraw() override;
	Screen *ProcessEvent(int event) override;

private:
	bool _writingIp;
	bool _writingPort;
	bool _writingKey;

	String _ip;
	String _port;
	String _serverKeyHex;

	String _status;
};

#endif
