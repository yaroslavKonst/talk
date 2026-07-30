#ifndef _CONTACT_LIST_SCREEN_HPP
#define _CONTACT_LIST_SCREEN_HPP

#include "Screen.hpp"
#include "Root.hpp"
#include "UiHelpers.hpp"
#include "../Message/ContactStorage.hpp"
#include "../Protocol/SessionParser.hpp"

class ContactListScreen :
	public Screen,
	public NetworkEventProcessor::ContactListProcessor
{
public:
	ContactListScreen(Root *root);
	~ContactListScreen();

	void Redraw() override;
	Screen *ProcessEvent(int event) override;

	CowBuffer<String> GetControlHelp();

	void ProcessContactList(
		bool success,
		const CommandListContacts::Response &contactList) override;

private:
	Root *_root;
	ContactStorage *_contacts;
	String _myName;

	bool _receivedContactList;
	bool _contactRequestSuccess;

	int _currentContact;
	CommandListContacts::Response _contactList;
	void RedrawContactList();
	void DrawNotification(String caption, String message);

	Screen *ProcessListEvent(int event);
	Screen *ProcessManageEvent(int event);

	void DrawManageWindow();

	enum class Mode
	{
		List,
		Manage
	};

	Mode _mode;

	enum class ManageMode
	{
		AddContact,
		ValidateKey
	};

	ManageMode _manageMode;

	void RequestContactList();
};

#endif
