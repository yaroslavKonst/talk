#ifndef _CONTACT_MANAGE_SCREEN_HPP
#define _CONTACT_MANAGE_SCREEN_HPP

#include "Screen.hpp"
#include "Root.hpp"
#include "UiHelpers.hpp"
#include "../Message/ContactStorage.hpp"

class ContactManageScreen : public Screen
{
public:
	ContactManageScreen(Root *root, String contactName);
	~ContactManageScreen();

	void Redraw() override;
	Screen *ProcessEvent(int event) override;

	CowBuffer<String> GetControlHelp();

private:
	Root *_root;
	ContactStorage *_contacts;
	String _contactName;

	int32_t _currentKey;
	void RedrawKeyList();

	Screen *ProcessListEvent(int event);

	enum class Mode
	{
		List,
		Add,
		Manage
	};

	Mode _mode;

	UiHelpers::TextBox _newKeyHex;
	void DrawAddWindow();
	Screen *ProcessAddEvent(int event);

	void DrawManageKeyWindow();
	Screen *ProcessManageEvent(int event);

	enum class ManageMode
	{
		Validation,
		Block,
		Default
	};

	ManageMode _manageMode;
};

#endif
