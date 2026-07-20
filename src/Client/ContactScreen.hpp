#ifndef _CONTACT_SCREEN_HPP
#define _CONTACT_SCREEN_HPP

#include "Screen.hpp"
#include "Root.hpp"
#include "UiHelpers.hpp"
#include "../Message/ContactStorage.hpp"

class ContactScreen : public Screen
{
public:
	ContactScreen(Root *root);
	~ContactScreen();

	void Redraw() override;
	Screen *ProcessEvent(int event) override;

	CowBuffer<String> GetControlHelp();

private:
	Root *_root;
	ContactStorage *_contacts;

	String _currentContact;
	void RedrawContactList();

	Screen *ProcessListEvent(int event);

	enum class Mode
	{
		List,
		Add,
	};

	Mode _mode;

	UiHelpers::TextBox _newContactName;
	void DrawAddWindow();
	Screen *ProcessAddEvent(int event);
};

#endif
