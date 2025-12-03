#include "ControlStorage.hpp"

#include <curses.h>

#include "../Common/File.hpp"
#include "../Common/Hex.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

int ControlStorage::Parse(String key)
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

ControlStorage::ControlStorage(const uint8_t *key) :
	_configFile("storage/" +
		DataToHex(key, KEY_SIZE) + "/talk.keys")
{
	InitConfig(key);
	Load();
}

ControlStorage::~ControlStorage()
{
}

namespace General
{
	static const char *Section = "General";

	static const char *Exit = "ExitProgram";
	static const char *Connect = "ConnectToServer";
}

namespace ChatList
{
	static const char *Section = "ContactList";

	static const char *Up = "PreviousContact";
	static const char *Down = "NextContact";
	static const char *Select = "SelectContact";
	static const char *Update = "UpdateContactList";
}

namespace MessageList
{
	static const char *Section = "Coversation";

	static const char *Up = "PreviousMessage";
	static const char *Down = "NextMessage";
	static const char *Back = "BackToContactList";
	static const char *NewMessage = "WriteNewMessage";
	static const char *GetAttachment = "ExtractAttachment";
}

namespace Typing
{
	static const char *Section = "Message";

	static const char *Send = "SendMessage";
	static const char *Back = "BackToConversation";
	static const char *Attachment = "ManageAttachment";
	static const char *CursorLeft = "MoveCursorLeft";
	static const char *CursorRight = "MoveCursorRight";
}

namespace Login
{
	static const char *Section = "Login";

	static const char *Up = "PreviousField";
	static const char *Down = "NextField";
	static const char *Back = "ExitLoginScreen";
	static const char *Connect = "ConnectToServer";
}

namespace Voice
{
	static const char *Section = "Voice";

	static const char *Start = "BeginVoiceCall";
	static const char *End = "EndVoiceCall";
	static const char *Accept = "AcceptCall";
	static const char *Decline = "DeclineCall";
	static const char *Mute = "MuteOutgoingAudio";
	static const char *EnterSettings = "EnterSettings";
	static const char *ExitSettings = "ExitSettings";
	static const char *VolumeInc = "IncreaseVolume";
	static const char *VolumeDec = "DecreaseVolume";
	static const char *SilenceInc = "IncreaseSilenceLevel";
	static const char *SilenceDec = "DecreaseSilenceLevel";
	static const char *FilterUp = "EnableFilter";
	static const char *FilterDown = "DisableFilter";
}

namespace Attachment
{
	static const char *Section = "Attachment";

	static const char *Clear = "RemoveAttachment";
	static const char *Back = "ExitAttachmentScreen";
	static const char *Proceed = "Confirm";
}

namespace Notification
{
	static const char *Section = "Notification";

	static const char *Confirm = "Confirm";
}

void ControlStorage::InitConfig(const uint8_t *key)
{
	if (!FileExists(_configFile.GetPath())) {
		CreateDirectory("storage");
		CreateDirectory("storage/" + DataToHex(key, KEY_SIZE));

		{
			using namespace General;
			_configFile.Set(Section, Exit, "End");
			_configFile.Set(Section, Connect, "Home");
		}

		{
			using namespace ChatList;
			_configFile.Set(Section, Up, "Up");
			_configFile.Set(Section, Down, "Down");
			_configFile.Set(Section, Select, "Enter");
			_configFile.Set(Section, Update, "Ctrl-U");
		}

		{
			using namespace MessageList;
			_configFile.Set(Section, Up, "Up");
			_configFile.Set(Section, Down, "Down");
			_configFile.Set(Section, Back, "Escape");
			_configFile.Set(Section, NewMessage, "Enter");
			_configFile.Set(Section, GetAttachment, "Ctrl-E");
		}

		{
			using namespace Typing;
			_configFile.Set(Section, Send, "Ctrl-S");
			_configFile.Set(Section, Back, "Escape");
			_configFile.Set(Section, Typing::Attachment, "Ctrl-A");
			_configFile.Set(Section, CursorLeft, "Left");
			_configFile.Set(Section, CursorRight, "Right");
		}

		{
			using namespace Login;
			_configFile.Set(Section, Up, "Up");
			_configFile.Set(Section, Down, "Down");
			_configFile.Set(Section, Back, "End");
			_configFile.Set(Section, Connect, "Enter");
		}

		{
			using namespace Voice;
			_configFile.Set(Section, Start, "Ctrl-V");
			_configFile.Set(Section, End, "Ctrl-V");
			_configFile.Set(Section, Accept, "y");
			_configFile.Set(Section, Decline, "n");
			_configFile.Set(Section, Mute, "Ctrl-B");
			_configFile.Set(Section, EnterSettings, "Ctrl-N");
			_configFile.Set(Section, ExitSettings, "End");
			_configFile.Set(Section, VolumeInc, "q");
			_configFile.Set(Section, VolumeDec, "a");
			_configFile.Set(Section, SilenceInc, "w");
			_configFile.Set(Section, SilenceDec, "s");
			_configFile.Set(Section, FilterUp, "e");
			_configFile.Set(Section, FilterDown, "d");
		}

		{
			using namespace Attachment;
			_configFile.Set(Section, Clear, "Delete");
			_configFile.Set(Section, Back, "End");
			_configFile.Set(Section, Proceed, "Enter");
		}

		{
			using namespace Notification;
			_configFile.Set(Section, Confirm, "Enter");
		}

		_configFile.Write();
	}
}

void ControlStorage::Load()
{
	{
		using namespace General;
		_keyNames[WorkExit] = _configFile.Get(Section, Exit);
		_keyNames[WorkConnect] = _configFile.Get(Section, Connect);
	}

	{
		using namespace ChatList;
		_keyNames[WorkListUp] = _configFile.Get(Section, Up);
		_keyNames[WorkListDown] = _configFile.Get(Section, Down);
		_keyNames[WorkListSelect] = _configFile.Get(Section, Select);
		_keyNames[WorkListUpdate] = _configFile.Get(Section, Update);
	}

	{
		using namespace MessageList;
		_keyNames[WorkChatUp] = _configFile.Get(Section, Up);
		_keyNames[WorkChatDown] = _configFile.Get(Section, Down);
		_keyNames[WorkChatBack] = _configFile.Get(Section, Back);
		_keyNames[WorkChatType] = _configFile.Get(Section, NewMessage);
		_keyNames[WorkChatExtract] =
			_configFile.Get(Section, GetAttachment);
	}

	{
		using namespace Typing;
		_keyNames[WorkTypeSend] = _configFile.Get(Section, Send);
		_keyNames[WorkTypeBack] = _configFile.Get(Section, Back);
		_keyNames[WorkTypeAttach] =
			_configFile.Get(Section, Typing::Attachment);
		_keyNames[WorkCursorLeft] =
			_configFile.Get(Section, CursorLeft);
		_keyNames[WorkCursorRight] =
			_configFile.Get(Section, CursorRight);
	}

	{
		using namespace Login;
		_keyNames[LoginUp] = _configFile.Get(Section, Up);
		_keyNames[LoginDown] = _configFile.Get(Section, Down);
		_keyNames[LoginBack] = _configFile.Get(Section, Back);
		_keyNames[LoginConnect] = _configFile.Get(Section, Connect);
	}

	{
		using namespace Voice;
		_keyNames[VoiceStart] = _configFile.Get(Section, Start);
		_keyNames[VoiceEnd] = _configFile.Get(Section, End);
		_keyNames[VoiceAccept] = _configFile.Get(Section, Accept);
		_keyNames[VoiceDecline] = _configFile.Get(Section, Decline);
		_keyNames[VoiceMute] = _configFile.Get(Section, Mute);
		_keyNames[VoiceEnterSettings] =
			_configFile.Get(Section, EnterSettings);
		_keyNames[VoiceExitSettings] =
			_configFile.Get(Section, ExitSettings);
		_keyNames[VoiceVolumeInc] = _configFile.Get(Section, VolumeInc);
		_keyNames[VoiceVolumeDec] = _configFile.Get(Section, VolumeDec);
		_keyNames[VoiceSilenceInc] =
			_configFile.Get(Section, SilenceInc);
		_keyNames[VoiceSilenceDec] =
			_configFile.Get(Section, SilenceDec);
		_keyNames[VoiceFilterUp] = _configFile.Get(Section, FilterUp);
		_keyNames[VoiceFilterDown] =
			_configFile.Get(Section, FilterDown);
	}

	{
		using namespace Attachment;
		_keyNames[AttachClear] = _configFile.Get(Section, Clear);
		_keyNames[AttachBack] = _configFile.Get(Section, Back);
		_keyNames[AttachProceed] = _configFile.Get(Section, Proceed);
	}

	{
		using namespace Notification;
		_keyNames[NotificationConfirm] =
			_configFile.Get(Section, Confirm);
	}

	for (int i = 0; i <= NotificationConfirm; i++) {
		_keys[i] = Parse(_keyNames[i]);

		if (_keyNames[i].Length() == 1) {
			_keyNames[i] = "'" + _keyNames[i] + "'";
		}
	}
}
