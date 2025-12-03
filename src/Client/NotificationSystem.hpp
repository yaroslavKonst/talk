#ifndef _NOTIFICATION_SYSTEM_HPP
#define _NOTIFICATION_SYSTEM_HPP

#include "ControlStorage.hpp"
#include "../Common/MyString.hpp"

class NotifyRedrawHandler
{
public:
	virtual ~NotifyRedrawHandler()
	{ }

	virtual void NotifyRedraw() = 0;
};

class NotificationSystem
{
public:
	NotificationSystem(
		NotifyRedrawHandler *handler,
		ControlStorage *controls);
	~NotificationSystem();

	void Notify(String message);

	void Redraw();
	bool ProcessEvent(int event);

private:
	struct Notification
	{
		Notification *Next;
		String Message;
	};

	Notification *_first;
	Notification *_last;

	NotifyRedrawHandler *_handler;

	ControlStorage *_controls;
};

#endif
