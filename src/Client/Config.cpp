#include "Config.hpp"

#include <curses.h>

#include "../Common/Hex.hpp"
#include "../Common/File.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

namespace General
{
	static const char *Section = "General";

	static const char *Name = "Name";
	static const char *NameValue = "";

	static const char *HostName = "Host Name";
	static const char *HostNameValue = "";
}

namespace Connection
{
	static const char *Section = "Connection";

	static const char *Address = "Server Address";
	static const char *AddressValue = "";

	static const char *Port = "Server Port";
	static const char *PortValue = "6524";

	static const char *Key = "Server Key";
	static const char *KeyValue = "";

	static const char *Autoconnect = "Connect Automatically";
	static const char *AutoconnectValue = "Yes";
}

namespace WorkScreenControls
{
	static const char *Section = "Main Screen Controls";

	static const char *Exit = "Exit Program";
	static const char *ExitValue = "End";

	static const char *Connect = "Connect To Server";
	static const char *ConnectValue = "Home";

	static const char *Account = "Manage Account";
	static const char *AccountValue = "Ctrl-A";

	static const char *Contact = "Manage Contacts";
	static const char *ContactValue = "Ctrl-C";

	static const char *ListSelect = "Select Chat";
	static const char *ListSelectValue = "Enter";

	static const char *ListUp = "Previous Chat";
	static const char *ListUpValue = "Up";

	static const char *ListDown = "Next Chat";
	static const char *ListDownValue = "Down";

	static const char *ChatType = "Type New Message";
	static const char *ChatTypeValue = "Enter";

	static const char *ChatUp = "Scroll Messages Up";
	static const char *ChatUpValue = "Up";

	static const char *ChatDown = "Scroll Messages Down";
	static const char *ChatDownValue = "Down";

	static const char *ChatBack = "Exit Chat";
	static const char *ChatBackValue = "q";

	static const char *TypeSend = "Send Message";
	static const char *TypeSendValue = "Ctrl-S";

	static const char *TypeBack = "Exit Type Mode";
	static const char *TypeBackValue = "Escape";

	static const char *CursorLeft = "Move Cursor Left";
	static const char *CursorLeftValue = "Left";

	static const char *CursorRight = "Move Cursor Right";
	static const char *CursorRightValue = "Right";

	static const char *CursorUp = "Move Cursor Up";
	static const char *CursorUpValue = "Up";

	static const char *CursorDown = "Move Cursor Down";
	static const char *CursorDownValue = "Down";
}

namespace AccountScreenControls
{
	static const char *Section = "Account Screen Controls";

	static const char *Back = "Exit Screen";
	static const char *BackValue = "End";

	static const char *Enter = "Select Setting";
	static const char *EnterValue = "Enter";

	static const char *Up = "Previous Control";
	static const char *UpValue = "Up";

	static const char *Down = "Next Control";
	static const char *DownValue = "Down";
}

namespace LoginScreenControls
{
	static const char *Section = "Login Screen Controls";

	static const char *Up = "Previous Field";
	static const char *UpValue = "Up";

	static const char *Down = "Next Field";
	static const char *DownValue = "Down";

	static const char *Back = "Exit Login Screen";
	static const char *BackValue = "End";

	static const char *Connect = "Connect To Server";
	static const char *ConnectValue = "Enter";
}

namespace ContactScreenControls
{
	static const char *Section = "Contact Screen Controls";

	static const char *Back = "Exit Contact Screen";
	static const char *BackValue = "End";

	static const char *Up = "Previous Contact";
	static const char *UpValue = "Up";

	static const char *Down = "Next Contact";
	static const char *DownValue = "Down";

	static const char *Enter = "Enter";
	static const char *EnterValue = "Enter";

	static const char *New = "New Contact";
	static const char *NewValue = "Ctrl-N";

	static const char *ToChat = "Go To Chat";
	static const char *ToChatValue = "Ctrl-C";

	static const char *Block = "Block Contact";
	static const char *BlockValue = "Ctrl-B";

	static const char *Remove = "Remove Contact";
	static const char *RemoveValue = "Ctrl-R";

	static const char *List = "Request Contact List";
	static const char *ListValue = "Ctrl-U";
}

namespace NotificationControls
{
	static const char *Section = "Notification Controls";

	static const char *Confirm = "Confirm Notification";
	static const char *ConfirmValue = "y";
}

static int ParseKey(String key)
{
	if (!key.Length()) {
		THROW("Key name is empty.");
	}

	if (key.Length() == 1) {
		if (key.CStr()[0] < ' ' || key.CStr()[0] > '~') {
			THROW("Invalid character " + key + ".");
		}

		return key.CStr()[0];
	}

	if (key.Length() == 6 && key.Substring(0, 5) == "Ctrl-") {
		if (key.CStr()[5] < 'A' || key.CStr()[5] > 'Z') {
			THROW("Invalid letter in " + key + ".");
		}

		return key.CStr()[5] - 'A' + 1;
	}

	if (key == "Enter") {
		return '\n';
	}
	
	if (key == "Escape") {
		return '\e';
	}
	
	if (key == "Backspace") {
		return '\b';
	}

	if (key == "Break") {
		return KEY_BREAK;
	}

	if (key == "Down") {
		return KEY_DOWN;
	}

	if (key == "Up") {
		return KEY_UP;
	}

	if (key == "Left") {
		return KEY_LEFT;
	}

	if (key == "Right") {
		return KEY_RIGHT;
	}

	if (key == "Home") {
		return KEY_HOME;
	}

	if (key == "F1") {
		return KEY_F(1);
	}

	if (key == "F2") {
		return KEY_F(2);
	}

	if (key == "F3") {
		return KEY_F(3);
	}

	if (key == "F4") {
		return KEY_F(4);
	}

	if (key == "F5") {
		return KEY_F(5);
	}

	if (key == "F6") {
		return KEY_F(6);
	}

	if (key == "F7") {
		return KEY_F(7);
	}

	if (key == "F8") {
		return KEY_F(8);
	}

	if (key == "F9") {
		return KEY_F(9);
	}

	if (key == "F10") {
		return KEY_F(10);
	}

	if (key == "F11") {
		return KEY_F(11);
	}

	if (key == "F12") {
		return KEY_F(12);
	}

	if (key == "Delete") {
		return KEY_DC;
	}

	if (key == "Insert") {
		return KEY_IC;
	}

	if (key == "PageDown") {
		return KEY_NPAGE;
	}

	if (key == "PageUp") {
		return KEY_PPAGE;
	}

	if (key == "Print") {
		return KEY_PRINT;
	}

	if (key == "Numpad7") {
		return KEY_A1;
	}

	if (key == "Numpad9") {
		return KEY_A3;
	}

	if (key == "Numpad5") {
		return KEY_B2;
	}

	if (key == "Numpad1") {
		return KEY_C1;
	}

	if (key == "Numpad3") {
		return KEY_C3;
	}

	if (key == "Command") {
		return KEY_COMMAND;
	}

	if (key == "End") {
		return KEY_END;
	}

	THROW("Unknown key name " + key + ".");
}

Config::Config(const Crypto::X25519::PublicKeyContainer &publicKey) :
	_configFile(
		"storage/" +
		DataToHex(publicKey.Key, Crypto::X25519::KEY_SIZE) +
		"/talk.conf"),
	_publicKey(publicKey)
{
	Init();
	LoadControls();
}

void Config::Save()
{
	_configFile.Write();
}

String Config::GetName()
{
	return _configFile.Get(General::Section, General::Name);
}

void Config::SetName(String value)
{
	_configFile.Set(General::Section, General::Name, value);
}

String Config::GetHostName()
{
	return _configFile.Get(General::Section, General::HostName);
}

void Config::SetHostName(String value)
{
	_configFile.Set(General::Section, General::HostName, value);
}

String Config::GetServerAddress()
{
	return _configFile.Get(Connection::Section, Connection::Address);
}
void Config::SetServerAddress(String value)
{
	_configFile.Set(Connection::Section, Connection::Address, value);
}

String Config::GetServerPort()
{
	return _configFile.Get(Connection::Section, Connection::Port);
}

void Config::SetServerPort(String value)
{
	_configFile.Set(Connection::Section, Connection::Port, value);
}

String Config::GetServerKeyHex()
{
	return _configFile.Get(Connection::Section, Connection::Key);
}

void Config::SetServerKeyHex(String value)
{
	_configFile.Set(Connection::Section, Connection::Key, value);
}

bool Config::GetAutoconnect()
{
	String rawValue = _configFile.Get(
		Connection::Section,
		Connection::Autoconnect);

	rawValue = rawValue.ToLowerCase();

	if (rawValue == "yes") {
		return true;
	}

	if (rawValue == "no") {
		return false;
	}

	THROW(String("Invalid config: ") + Connection::Autoconnect +
		" must be 'Yes' or 'No'.");
}

void Config::SetAutoconnect(bool value)
{
	_configFile.Set(
		Connection::Section,
		Connection::Autoconnect,
		value ? "Yes" : "No");
}

void Config::Init()
{
	if (!FileExists(_configFile.GetPath())) {
		CreateDirectory("storage");
		CreateDirectory(
			"storage/" +
			DataToHex(_publicKey.Key, Crypto::X25519::KEY_SIZE));

		{
			using namespace General;
			_configFile.Set(Section, Name, NameValue);
			_configFile.Set(Section, HostName, HostNameValue);
		}

		{
			using namespace Connection;
			_configFile.Set(Section, Address, AddressValue);
			_configFile.Set(Section, Port, PortValue);
			_configFile.Set(Section, Key, KeyValue);
			_configFile.Set(Section, Autoconnect, AutoconnectValue);
		}

		{
			using namespace WorkScreenControls;
			_configFile.Set(Section, Exit, ExitValue);
			_configFile.Set(Section, Connect, ConnectValue);
			_configFile.Set(Section, Account, AccountValue);
			_configFile.Set(Section, Contact, ContactValue);
			_configFile.Set(Section, ListSelect, ListSelectValue);
			_configFile.Set(Section, ListUp, ListUpValue);
			_configFile.Set(Section, ListDown, ListDownValue);
			_configFile.Set(Section, ChatType, ChatTypeValue);
			_configFile.Set(Section, ChatUp, ChatUpValue);
			_configFile.Set(Section, ChatDown, ChatDownValue);
			_configFile.Set(Section, ChatBack, ChatBackValue);
			_configFile.Set(Section, TypeSend, TypeSendValue);
			_configFile.Set(Section, TypeBack, TypeBackValue);
			_configFile.Set(Section, CursorLeft, CursorLeftValue);
			_configFile.Set(Section, CursorRight, CursorRightValue);
			_configFile.Set(Section, CursorUp, CursorUpValue);
			_configFile.Set(Section, CursorDown, CursorDownValue);
		}

		{
			using namespace AccountScreenControls;
			_configFile.Set(Section, Back, BackValue);
			_configFile.Set(Section, Enter, EnterValue);
			_configFile.Set(Section, Up, UpValue);
			_configFile.Set(Section, Down, DownValue);
		}

		{
			using namespace LoginScreenControls;
			_configFile.Set(Section, Up, UpValue);
			_configFile.Set(Section, Down, DownValue);
			_configFile.Set(Section, Back, BackValue);
			_configFile.Set(Section, Connect, ConnectValue);
		}

		{
			using namespace ContactScreenControls;
			_configFile.Set(Section, Back, BackValue);
			_configFile.Set(Section, Down, DownValue);
			_configFile.Set(Section, Up, UpValue);
			_configFile.Set(Section, Enter, EnterValue);
			_configFile.Set(Section, New, NewValue);
			_configFile.Set(Section, ToChat, ToChatValue);
			_configFile.Set(Section, Block, BlockValue);
			_configFile.Set(Section, Remove, RemoveValue);
			_configFile.Set(Section, List, ListValue);
		}

		{
			using namespace NotificationControls;
			_configFile.Set(Section, Confirm, ConfirmValue);
		}

		_configFile.Set("voice", "Volume", "100");
		_configFile.Set("voice", "ApplyFilter", "Yes");
		_configFile.Set("voice", "SilenceLevel", "3");
		_configFile.Write();
	}
}

void Config::LoadControls()
{
	for (int i = 0; i <= (int)Keys::NotificationConfirm; i++) {
		_keyNames[i] = "Numpad1";
	}

	{
		using namespace WorkScreenControls;
		_keyNames[(int)Keys::WorkExit] = _configFile.Get(Section, Exit);
		_keyNames[(int)Keys::WorkConnect] =
			_configFile.Get(Section, Connect);
		_keyNames[(int)Keys::WorkAccount] =
			_configFile.Get(Section, Account);
		_keyNames[(int)Keys::WorkContact] =
			_configFile.Get(Section, Contact);
		_keyNames[(int)Keys::WorkListSelect] =
			_configFile.Get(Section, ListSelect);
		_keyNames[(int)Keys::WorkListUp] =
			_configFile.Get(Section, ListUp);
		_keyNames[(int)Keys::WorkListDown] =
			_configFile.Get(Section, ListDown);
		_keyNames[(int)Keys::WorkChatType] =
			_configFile.Get(Section, ChatType);
		_keyNames[(int)Keys::WorkChatUp] =
			_configFile.Get(Section, ChatUp);
		_keyNames[(int)Keys::WorkChatDown] =
			_configFile.Get(Section, ChatDown);
		_keyNames[(int)Keys::WorkChatBack] =
			_configFile.Get(Section, ChatBack);
		_keyNames[(int)Keys::WorkTypeSend] =
			_configFile.Get(Section, TypeSend);
		_keyNames[(int)Keys::WorkTypeBack] =
			_configFile.Get(Section, TypeBack);
		_keyNames[(int)Keys::WorkCursorLeft] =
			_configFile.Get(Section, CursorLeft);
		_keyNames[(int)Keys::WorkCursorRight] =
			_configFile.Get(Section, CursorRight);
		_keyNames[(int)Keys::WorkCursorUp] =
			_configFile.Get(Section, CursorUp);
		_keyNames[(int)Keys::WorkCursorDown] =
			_configFile.Get(Section, CursorDown);
	}

	{
		using namespace AccountScreenControls;
		_keyNames[(int)Keys::AccountBack] =
			_configFile.Get(Section, Back);
		_keyNames[(int)Keys::AccountEnter] =
			_configFile.Get(Section, Enter);
		_keyNames[(int)Keys::AccountUp] =
			_configFile.Get(Section, Up);
		_keyNames[(int)Keys::AccountDown] =
			_configFile.Get(Section, Down);
	}

	{
		using namespace LoginScreenControls;
		_keyNames[(int)Keys::LoginUp] =
			_configFile.Get(Section, Up);
		_keyNames[(int)Keys::LoginDown] =
			_configFile.Get(Section, Down);
		_keyNames[(int)Keys::LoginBack] =
			_configFile.Get(Section, Back);
		_keyNames[(int)Keys::LoginConnect] =
			_configFile.Get(Section, Connect);
	}

	{
		using namespace ContactScreenControls;
		_keyNames[(int)Keys::ContactBack] =
			_configFile.Get(Section, Back);
		_keyNames[(int)Keys::ContactUp] =
			_configFile.Get(Section, Up);
		_keyNames[(int)Keys::ContactDown] =
			_configFile.Get(Section, Down);
		_keyNames[(int)Keys::ContactEnter] =
			_configFile.Get(Section, Enter);
		_keyNames[(int)Keys::ContactNew] =
			_configFile.Get(Section, New);
		_keyNames[(int)Keys::ContactToChat] =
			_configFile.Get(Section, ToChat);
		_keyNames[(int)Keys::ContactBlock] =
			_configFile.Get(Section, Block);
		_keyNames[(int)Keys::ContactRemove] =
			_configFile.Get(Section, Remove);
		_keyNames[(int)Keys::ContactListContacts] =
			_configFile.Get(Section, List);
	}

	{
		using namespace NotificationControls;
		_keyNames[(int)Keys::NotificationConfirm] =
			_configFile.Get(Section, Confirm);
	}

	for (int i = 0; i <= (int)Keys::NotificationConfirm; i++) {
		try {
			_keys[i] = ParseKey(_keyNames[i]);
		} catch (Exception &ex) {
			THROW("Failed to parse key " + ToString(i) + ".");
		}

		if (_keyNames[i].Length() == 1) {
			_keyNames[i] = "'" + _keyNames[i] + "'";
		}
	}
}
