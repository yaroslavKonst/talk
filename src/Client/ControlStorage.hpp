#ifndef _CONTROL_STORAGE_HPP
#define _CONTROL_STORAGE_HPP

#include "../Common/IniFile.hpp"

class ControlStorage
{
public:
	ControlStorage(const uint8_t *key);
	~ControlStorage();

	int WorkExitKey()
	{
		return _keys[WorkExit];
	}

	String WorkExitName()
	{
		return _keyNames[WorkExit];
	}

	int WorkConnectKey()
	{
		return _keys[WorkConnect];
	}

	String WorkConnectName()
	{
		return _keyNames[WorkConnect];
	}

	int WorkListSelectKey()
	{
		return _keys[WorkListSelect];
	}

	String WorkListSelectName()
	{
		return _keyNames[WorkListSelect];
	}

	int WorkListUpKey()
	{
		return _keys[WorkListUp];
	}

	String WorkListUpName()
	{
		return _keyNames[WorkListUp];
	}

	int WorkListDownKey()
	{
		return _keys[WorkListDown];
	}

	String WorkListDownName()
	{
		return _keyNames[WorkListDown];
	}

	int WorkListUpdateKey()
	{
		return _keys[WorkListUpdate];
	}

	String WorkListUpdateName()
	{
		return _keyNames[WorkListUpdate];
	}

	int WorkChatTypeKey()
	{
		return _keys[WorkChatType];
	}

	String WorkChatTypeName()
	{
		return _keyNames[WorkChatType];
	}

	int WorkChatUpKey()
	{
		return _keys[WorkChatUp];
	}

	String WorkChatUpName()
	{
		return _keyNames[WorkChatUp];
	}

	int WorkChatDownKey()
	{
		return _keys[WorkChatDown];
	}

	String WorkChatDownName()
	{
		return _keyNames[WorkChatDown];
	}

	int WorkChatBackKey()
	{
		return _keys[WorkChatBack];
	}

	String WorkChatBackName()
	{
		return _keyNames[WorkChatBack];
	}

	int WorkChatExtractKey()
	{
		return _keys[WorkChatExtract];
	}

	String WorkChatExtractName()
	{
		return _keyNames[WorkChatExtract];
	}

	int WorkTypeSendKey()
	{
		return _keys[WorkTypeSend];
	}

	String WorkTypeSendName()
	{
		return _keyNames[WorkTypeSend];
	}

	int WorkTypeAttachKey()
	{
		return _keys[WorkTypeAttach];
	}

	String WorkTypeAttachName()
	{
		return _keyNames[WorkTypeAttach];
	}

	int WorkTypeBackKey()
	{
		return _keys[WorkTypeBack];
	}

	String WorkTypeBackName()
	{
		return _keyNames[WorkTypeBack];
	}


	int WorkCursorLeftKey()
	{
		return _keys[WorkCursorLeft];
	}

	String WorkCursorLeftName()
	{
		return _keyNames[WorkCursorLeft];
	}

	int WorkCursorRightKey()
	{
		return _keys[WorkCursorRight];
	}

	String WorkCursorRightName()
	{
		return _keyNames[WorkCursorRight];
	}

	int LoginBackKey()
	{
		return _keys[LoginBack];
	}

	String LoginBackName()
	{
		return _keyNames[LoginBack];
	}

	int LoginUpKey()
	{
		return _keys[LoginUp];
	}

	String LoginUpName()
	{
		return _keyNames[LoginUp];
	}

	int LoginDownKey()
	{
		return _keys[LoginDown];
	}

	String LoginDownName()
	{
		return _keyNames[LoginDown];
	}

	int LoginConnectKey()
	{
		return _keys[LoginConnect];
	}

	String LoginConnectName()
	{
		return _keyNames[LoginConnect];
	}

	int VoiceStartKey()
	{
		return _keys[VoiceStart];
	}

	String VoiceStartName()
	{
		return _keyNames[VoiceStart];
	}

	int VoiceEndKey()
	{
		return _keys[VoiceEnd];
	}

	String VoiceEndName()
	{
		return _keyNames[VoiceEnd];
	}

	int VoiceMuteKey()
	{
		return _keys[VoiceMute];
	}

	String VoiceMuteName()
	{
		return _keyNames[VoiceMute];
	}

	int VoiceEnterSettingsKey()
	{
		return _keys[VoiceEnterSettings];
	}

	String VoiceEnterSettingsName()
	{
		return _keyNames[VoiceEnterSettings];
	}

	int VoiceExitSettingsKey()
	{
		return _keys[VoiceExitSettings];
	}

	String VoiceExitSettingsName()
	{
		return _keyNames[VoiceExitSettings];
	}

	int VoiceVolumeIncKey()
	{
		return _keys[VoiceVolumeInc];
	}

	String VoiceVolumeIncName()
	{
		return _keyNames[VoiceVolumeInc];
	}

	int VoiceVolumeDecKey()
	{
		return _keys[VoiceVolumeDec];
	}

	String VoiceVolumeDecName()
	{
		return _keyNames[VoiceVolumeDec];
	}

	int VoiceSilenceIncKey()
	{
		return _keys[VoiceSilenceInc];
	}

	String VoiceSilenceIncName()
	{
		return _keyNames[VoiceSilenceInc];
	}

	int VoiceSilenceDecKey()
	{
		return _keys[VoiceSilenceDec];
	}

	String VoiceSilenceDecName()
	{
		return _keyNames[VoiceSilenceDec];
	}

	int VoiceFilterUpKey()
	{
		return _keys[VoiceFilterUp];
	}

	String VoiceFilterUpName()
	{
		return _keyNames[VoiceFilterUp];
	}

	int VoiceFilterDownKey()
	{
		return _keys[VoiceFilterDown];
	}

	String VoiceFilterDownName()
	{
		return _keyNames[VoiceFilterDown];
	}

	int VoiceAcceptKey()
	{
		return _keys[VoiceAccept];
	}

	String VoiceAcceptName()
	{
		return _keyNames[VoiceAccept];
	}

	int VoiceDeclineKey()
	{
		return _keys[VoiceDecline];
	}

	String VoiceDeclineName()
	{
		return _keyNames[VoiceDecline];
	}

	int AttachBackKey()
	{
		return _keys[AttachBack];
	}

	String AttachBackName()
	{
		return _keyNames[AttachBack];
	}

	int AttachClearKey()
	{
		return _keys[AttachClear];
	}

	String AttachClearName()
	{
		return _keyNames[AttachClear];
	}

	int AttachProceedKey()
	{
		return _keys[AttachProceed];
	}

	String AttachProceedName()
	{
		return _keyNames[AttachProceed];
	}

	int NotificationConfirmKey()
	{
		return _keys[NotificationConfirm];
	}

	String NotificationConfirmName()
	{
		return _keyNames[NotificationConfirm];
	}

private:
	enum FunctionNames
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

	IniFile _configFile;
	void InitConfig(const uint8_t *key);

	void Load();

	static int Parse(String key);

	int _keys[NotificationConfirm + 1];
	String _keyNames[NotificationConfirm + 1];
};

#endif
