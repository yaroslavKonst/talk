#ifndef _PASSWORD_SCREEN_HPP
#define _PASSWORD_SCREEN_HPP

#include "VoiceChat.hpp"
#include "Screen.hpp"

class PasswordScreen : public Screen
{
public:
	PasswordScreen(ClientSession *session, VoiceChat *voiceChat);

	void Redraw() override;
	Screen *ProcessEvent(int event) override;

private:
	String _password;
	String _status;

	void GenerateKeys();

	VoiceChat *_voiceChat;
};

#endif
