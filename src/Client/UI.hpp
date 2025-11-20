#ifndef _UI_HPP
#define _UI_HPP

#include <curses.h>

#include "Screen.hpp"
#include "VoiceChat.hpp"
#include "../Protocol/ClientSession.hpp"

class UI
{
public:
	UI(ClientSession *session);
	~UI();

	bool ProcessEvent();
	void ProcessResize();

	void Disconnect();

	int GetSoundReadFileDescriptor();
	void ProcessSound();

private:
	Screen *_screen;

	ClientSession *_session;

	VoiceChat _voiceChat;
};

#endif
