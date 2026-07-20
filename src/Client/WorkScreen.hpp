#ifndef _WORK_SCREEN_HPP
#define _WORK_SCREEN_HPP

#include "Screen.hpp"
#include "Root.hpp"

class WorkScreen : public Screen
{
public:
	WorkScreen(Root *root);
	~WorkScreen();

	void Redraw() override;
	Screen *ProcessEvent(int event) override;

	CowBuffer<String> GetControlHelp();

private:
	Root *_root;

	void RedrawFrames();
	void RedrawContactList();
	void RedrawCurrentChat();
	void RedrawTextBox();

	void ProcessChatListEvent(int event);
	void ProcessChatScreenEvent(int event);
};

#endif
