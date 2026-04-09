#ifndef _CONTACT_SCREEN_HPP
#define _CONTACT_SCREEN_HPP

#include "Screen.hpp"
#include "Root.hpp"
#include "../Message/ContactStorage.hpp"

class ContactScreen : public Screen
{
public:
	ContactScreen(Root *root);
	~ContactScreen();

	void Redraw() override;
	Screen *ProcessEvent(int event) override;

private:
	Root *_root;
	ContactStorage *_contacts;

	String _currentContact;
	void RedrawContactList();
};

#endif
