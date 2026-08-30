#ifndef _ACCOUNT_SCREEN_HPP
#define _ACCOUNT_SCREEN_HPP

#include "Screen.hpp"
#include "Root.hpp"
#include "UiHelpers.hpp"

class AccountScreen :
	public Screen,
	public NetworkEventProcessor::AccountSettingsProcessor
{
public:
	AccountScreen(Root *root);
	~AccountScreen();

	void Redraw() override;
	Screen *ProcessEvent(int event) override;

	CowBuffer<String> GetControlHelp() override;

	void ReceiveAccountSettings(
		bool allowMessagesOnlyFromContactList,
		bool allowCallsOnlyFromContactList,
		bool showInContactList) override;

private:
	Root *_root;

	UiHelpers::TextBox _name;
	String _originalName;

	bool _receivedAccountSettingsFromServer;

	bool _onlyContactsCanWriteMessages;
	bool _onlyContactsCanCall;
	bool _showInContactList;

	enum class State
	{
		NameSetting,
		MessageSetting,
		CallSetting,
		ContactListSetting
	};

	State _state;
};

#endif
