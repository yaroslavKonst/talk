#ifndef _WORK_SCREEN_HPP
#define _WORK_SCREEN_HPP

#include "VoiceChat.hpp"
#include "NotificationSystem.hpp"
#include "ChatList.hpp"
#include "Screen.hpp"
#include "../Common/IniFile.hpp"

class WorkScreen :
	public Screen,
	public MessageProcessor,
	public NotifyRedrawHandler,
	public VoiceProcessor
{
public:
	WorkScreen(ClientSession *session, VoiceChat *voiceChat);
	~WorkScreen();

	void Redraw() override;
	Screen *ProcessEvent(int event) override;

	void NotifyDelivery(void *userPointer, int32_t status) override;
	void DeliverMessage(CowBuffer<uint8_t> message) override;
	void UpdateUserData(const uint8_t *key, String name) override;
	int64_t GetLatestReceiveTimestamp() override
	{
		return _chatList.GetLatestTimestamp();
	}

	void NotifyRedraw() override
	{
		Redraw();
	}

	void VoiceInit(const uint8_t *key);
	void VoiceRequest(const uint8_t *key, int64_t timestamp) override;
	void VoiceInitResponse(int32_t code) override;
	void VoiceEnd() override;
	void EndVoice() override;
	void VoiceRedrawRequested() override;
	void ReceiveVoiceFrame(CowBuffer<uint8_t> frame) override;
	void SendVoiceFrame(CowBuffer<uint8_t> frame) override;
	void AnswerVoiceRequest(bool accept) override;

private:
	NotificationSystem _notificationSystem;

	Screen *_overlay;

	void Connect();

	ChatList _chatList;
	Chat *_activeChat;

	void ProcessChatListEvent(int event);
	void ProcessChatScreenEvent(int event);

	VoiceChat *_voiceChat;

	void DrawHelp();

	IniFile _configFile;
	void InitConfigFile();
};

#endif
