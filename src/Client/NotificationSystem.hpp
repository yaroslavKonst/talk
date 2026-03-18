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
	void *BlockNotify(String message);
	void BlockCancel(void *handle);

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

	Notification *_blockFirst;
	Notification *_blockLast;

	Root *_root;
};

#endif
