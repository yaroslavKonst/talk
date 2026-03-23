#ifndef _CONFIG_HPP
#define _CONFIG_HPP

#include "../Common/IniFile.hpp"

class Config
{
public:
	Config(const uint8_t *publicKey);

	void Save();

	String GetName();
	void SetName(String value);

	String GetServerAddress();
	void SetServerAddress(String value);

	String GetServerPort();
	void SetServerPort(String value);

	String GetServerKeyHex();
	void SetServerKeyHex(String value);

	// Controls.
	int WorkExitKey()
	{
		return _keys[(int)Keys::WorkExit];
	}

	String WorkExitName()
	{
		return _keyNames[(int)Keys::WorkExit];
	}

	int WorkConnectKey()
	{
		return _keys[(int)Keys::WorkConnect];
	}

	String WorkConnectName()
	{
		return _keyNames[(int)Keys::WorkConnect];
	}

	int LoginBackKey()
	{
		return _keys[(int)Keys::LoginBack];
	}

	String LoginBackName()
	{
		return _keyNames[(int)Keys::LoginBack];
	}

	int LoginUpKey()
	{
		return _keys[(int)Keys::LoginUp];
	}

	String LoginUpName()
	{
		return _keyNames[(int)Keys::LoginUp];
	}

	int LoginDownKey()
	{
		return _keys[(int)Keys::LoginDown];
	}

	String LoginDownName()
	{
		return _keyNames[(int)Keys::LoginDown];
	}

	int LoginConnectKey()
	{
		return _keys[(int)Keys::LoginConnect];
	}

	String LoginConnectName()
	{
		return _keyNames[(int)Keys::LoginConnect];
	}

	int NotificationConfirmKey()
	{
		return _keys[(int)Keys::NotificationConfirm];
	}

	String NotificationConfirmName()
	{
		return _keyNames[(int)Keys::NotificationConfirm];
	}

private:
	IniFile _configFile;
	const uint8_t *_publicKey;

	void Init();
	void LoadControls();

	enum class Keys
	{
		WorkExit = 0,
		WorkConnect = 1,

		WorkListSelect = 2,
		WorkListUp = 3,
		WorkListDown = 4,
		WorkListUpdate = 5,

		WorkChatType = 6,
		WorkChatUp = 7,
		WorkChatDown = 8,
		WorkChatBack = 9,
		WorkChatExtract = 10,

		WorkTypeSend = 11,
		WorkTypeAttach = 12,
		WorkTypeBack = 13,
		WorkCursorLeft = 14,
		WorkCursorRight = 15,

		LoginBack = 16,
		LoginUp = 17,
		LoginDown = 18,
		LoginConnect = 19,

		VoiceStart = 20,
		VoiceEnd = 21,
		VoiceMute = 22,
		VoiceEnterSettings = 23,
		VoiceExitSettings = 24,
		VoiceVolumeInc = 25,
		VoiceVolumeDec = 26,
		VoiceSilenceInc = 27,
		VoiceSilenceDec = 28,
		VoiceFilterUp = 29,
		VoiceFilterDown = 30,
		VoiceAccept = 31,
		VoiceDecline = 32,

		AttachBack = 33,
		AttachClear = 34,
		AttachProceed = 35,

		NotificationConfirm = 36
	};

	int _keys[(int)Keys::NotificationConfirm + 1];
	String _keyNames[(int)Keys::NotificationConfirm + 1];
};

#endif
