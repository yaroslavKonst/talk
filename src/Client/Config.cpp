#include "Config.hpp"

#include <curses.h>

#include "../Common/Hex.hpp"
#include "../Common/File.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

namespace Connection
{
	const char *Section = "Connection";

	const char *Address = "ServerAddress";
	const char *AddressValue = "";

	const char *Port = "ServerPort";
	const char *PortValue = "6524";

	const char *Key = "ServerKey";
	const char *KeyValue = "";
}

namespace WorkScreenControls
{
	const char *Section = "MainScreenControls";

	const char *Exit = "ExitProgram";
	const char *ExitValue = "End";

	const char *Connect = "ConnectToServer";
	const char *ConnectValue = "Home";
}

namespace LoginScreenControls
{
	const char *Section = "LoginScreenControls";

	static const char *Up = "PreviousField";
	static const char *UpValue = "Up";

	static const char *Down = "NextField";
	static const char *DownValue = "Down";

	static const char *Back = "ExitLoginScreen";
	static const char *BackValue = "End";

	static const char *Connect = "ConnectToServer";
	static const char *ConnectValue = "Enter";
}

namespace NotificationControls
{
	const char *Section = "NotificationControls";

	const char *Confirm = "ConfirmNotification";
	const char *ConfirmValue = "y";
}

static int ParseKey(String key)
{
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

Config::Config(const uint8_t *publicKey) :
	_configFile(
		"storage/" +
		DataToHex(publicKey, KEY_SIZE) +
		"/talk.conf")
{
	_publicKey = publicKey;

	Init();
	LoadControls();
}

void Config::Save()
{
	_configFile.Write();
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

void Config::Init()
{
	if (!FileExists(_configFile.GetPath())) {
		CreateDirectory("storage");
		CreateDirectory(
			"storage/" +
			DataToHex(_publicKey, KEY_SIZE));

		{
			using namespace Connection;
			_configFile.Set(Section, Address, AddressValue);
			_configFile.Set(Section, Port, PortValue);
			_configFile.Set(Section, Key, KeyValue);
		}

		{
			using namespace WorkScreenControls;
			_configFile.Set(Section, Exit, ExitValue);
			_configFile.Set(Section, Connect, ConnectValue);
		}

		{
			using namespace LoginScreenControls;
			_configFile.Set(Section, Up, UpValue);
			_configFile.Set(Section, Down, DownValue);
			_configFile.Set(Section, Back, BackValue);
			_configFile.Set(Section, Connect, ConnectValue);
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
		using namespace NotificationControls;
		_keyNames[(int)Keys::NotificationConfirm] =
			_configFile.Get(Section, Confirm);
	}

	for (int i = 0; i <= (int)Keys::NotificationConfirm; i++) {
		_keys[i] = ParseKey(_keyNames[i]);

		if (_keyNames[i].Length() == 1) {
			_keyNames[i] = "'" + _keyNames[i] + "'";
		}
	}
}
