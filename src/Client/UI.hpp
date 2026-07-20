#ifndef _UI_HPP
#define _UI_HPP

#include "Root.hpp"
#include "Screen.hpp"
#include "NotificationSystem.hpp"
#include "../Common/Exception.hpp"

class UI :
	public UIEventProcessor,
	public DescriptorEventProcessor
{
public:
	UI(Root *root);
	~UI();

	int GetDescriptor() override
	{
		return 0;
	}

	bool RequestRead() override
	{
		return true;
	}

	bool RequestWrite() override
	{
		return false;
	}

	void ProcessRead() override;

	void ProcessWrite() override
	{
		THROW("This method must never be called.");
	}

	bool ProcessEvent();
	void ProcessResize();
	void Redraw() override;

	void Notify(String message) override;
	void *BlockNotify(String message) override;
	void BlockCancel(void *handle) override;

private:
	Root *_root;

	NotificationSystem _notifier;

	int _rows;
	int _columns;

	void DrawUserData();
	void DrawConnectionState();
	void DrawVoiceState();
	void DrawControlHelp();

	struct ScreenStackEntry
	{
		Screen *screen;
		ScreenStackEntry *previous;

		ScreenStackEntry(ScreenStackEntry *prev)
		{
			previous = prev;
			screen = nullptr;
		}
	};

	ScreenStackEntry *_screenStack;
};

#endif
