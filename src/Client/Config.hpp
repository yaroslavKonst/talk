#ifndef _CONFIG_HPP
#define _CONFIG_HPP

#include "../Common/IniFile.hpp"
#include "../Crypto/Crypto.hpp"

class Config
{
public:
	Config(const Crypto::X25519::PublicKeyContainer &publicKey);

	void Save();

	String GetName();
	void SetName(String value);

	String GetHostName();
	void SetHostName(String value);

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

	int WorkContactKey()
	{
		return _keys[(int)Keys::WorkContact];
	}

	String WorkContactName()
	{
		return _keyNames[(int)Keys::WorkContact];
	}

	int WorkListSelectKey()
	{
		return _keys[(int)Keys::WorkListSelect];
	}

	String WorkListSelectName()
	{
		return _keyNames[(int)Keys::WorkListSelect];
	}

	int WorkListUpKey()
	{
		return _keys[(int)Keys::WorkListUp];
	}

	String WorkListUpName()
	{
		return _keyNames[(int)Keys::WorkListUp];
	}

	int WorkListDownKey()
	{
		return _keys[(int)Keys::WorkListDown];
	}

	String WorkListDownName()
	{
		return _keyNames[(int)Keys::WorkListDown];
	}

	int WorkChatBackKey()
	{
		return _keys[(int)Keys::WorkChatBack];
	}

	String WorkChatBackName()
	{
		return _keyNames[(int)Keys::WorkChatBack];
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

	int ContactBackKey()
	{
		return _keys[(int)Keys::ContactBack];
	}

	String ContactBackName()
	{
		return _keyNames[(int)Keys::ContactBack];
	}

	int ContactUpKey()
	{
		return _keys[(int)Keys::ContactUp];
	}

	String ContactUpName()
	{
		return _keyNames[(int)Keys::ContactUp];
	}

	int ContactDownKey()
	{
		return _keys[(int)Keys::ContactDown];
	}

	String ContactDownName()
	{
		return _keyNames[(int)Keys::ContactDown];
	}

	int ContactEnterKey()
	{
		return _keys[(int)Keys::ContactEnter];
	}

	String ContactEnterName()
	{
		return _keyNames[(int)Keys::ContactEnter];
	}

	int ContactNewKey()
	{
		return _keys[(int)Keys::ContactNew];
	}

	String ContactNewName()
	{
		return _keyNames[(int)Keys::ContactNew];
	}

	int ContactToChatKey()
	{
		return _keys[(int)Keys::ContactToChat];
	}

	String ContactToChatName()
	{
		return _keyNames[(int)Keys::ContactToChat];
	}

	int ContactBlockKey()
	{
		return _keys[(int)Keys::ContactBlock];
	}

	String ContactBlockName()
	{
		return _keyNames[(int)Keys::ContactBlock];
	}

	int ContactListContactsKey()
	{
		return _keys[(int)Keys::ContactListContacts];
	}

	String ContactListContactsName()
	{
		return _keyNames[(int)Keys::ContactListContacts];
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
	const Crypto::X25519::PublicKeyContainer &_publicKey;

	void Init();
	void LoadControls();

	enum class Keys
	{
		WorkExit,
		WorkConnect,
		WorkContact,

		WorkListSelect,
		WorkListUp,
		WorkListDown,

		WorkChatType,
		WorkChatUp,
		WorkChatDown,
		WorkChatBack,
		WorkChatExtract,

		WorkTypeSend,
		WorkTypeAttach,
		WorkTypeBack,
		WorkCursorLeft,
		WorkCursorRight,

		LoginBack,
		LoginUp,
		LoginDown,
		LoginConnect,

		ContactBack,
		ContactUp,
		ContactDown,
		ContactEnter,
		ContactNew,
		ContactToChat,
		ContactBlock,
		ContactListContacts,

		VoiceStart,
		VoiceEnd,
		VoiceMute,
		VoiceEnterSettings,
		VoiceExitSettings,
		VoiceVolumeInc,
		VoiceVolumeDec,
		VoiceSilenceInc,
		VoiceSilenceDec,
		VoiceFilterUp,
		VoiceFilterDown,
		VoiceAccept,
		VoiceDecline,

		AttachBack,
		AttachClear,
		AttachProceed,

		NotificationConfirm
	};

	int _keys[(int)Keys::NotificationConfirm + 1];
	String _keyNames[(int)Keys::NotificationConfirm + 1];
};

#endif
