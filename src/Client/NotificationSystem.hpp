#ifndef _NOTIFICATION_SYSTEM_HPP
#define _NOTIFICATION_SYSTEM_HPP

#include "Root.hpp"
#include "ControlStorage.hpp"
#include "../Common/MyString.hpp"

class NotificationSystem
{
public:
	NotificationSystem(Root *root);
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

	Root *_root;
};

#endif
