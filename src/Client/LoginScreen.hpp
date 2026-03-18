#ifndef _LOGIN_SCREEN_HPP
#define _LOGIN_SCREEN_HPP

#include "Screen.hpp"
#include "Root.hpp"
#include "UiHelpers.hpp"

class LoginScreen : public Screen
{
public:
	LoginScreen(Root *root);

	void Redraw() override;
	Screen *ProcessEvent(int event) override;

private:
	bool _writingIp;
	bool _writingPort;
	bool _writingKey;

	UiHelpers::TextBox _ip;
	UiHelpers::TextBox _port;
	UiHelpers::TextBox _serverKeyHex;

	bool _modified;

	Root *_root;

	Screen *ProcessConnection();
};

#endif
