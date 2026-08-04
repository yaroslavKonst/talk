#ifndef _WORK_SCREEN_HPP
#define _WORK_SCREEN_HPP

#include "Screen.hpp"
#include "Root.hpp"
#include "MessageDraft.hpp"

class WorkScreen : public Screen
{
public:
	WorkScreen(Root *root);
	~WorkScreen();

	void Redraw() override;
	Screen *ProcessEvent(int event) override;

	CowBuffer<String> GetControlHelp();

	void ActivateCurrentChat();

private:
	Root *_root;

	void RedrawFrames();
	void RedrawChatList();

	void RedrawCurrentChat();

	// Chat screen draw helpers.
	bool AddLineToChatScreen(
		int &currentLinePosition,
		int &skipLines,
		String text,
		bool centering = false);
	bool RedrawConversationStart(int &currentLinePosition, int &skipLines);
	bool RedrawMessageBody(
		int &currentLinePosition,
		int &skipLines,
		MessageEventProcessor::MessageDescriptorBase *md);
	bool RedrawMessageHeader(
		int &currentLinePosition,
		int &skipLines,
		MessageEventProcessor::MessageDescriptorBase *md);
	bool RedrawMessageDelimiter(
		int &currentLinePosition,
		int &skipLines);

	void RedrawTextBox();

	Screen *ProcessChatListEvent(int event);
	Screen *ProcessChatScreenEvent(int event);

	struct ChatState
	{
		ChatState *Next;

		String PeerName;
		ObjectStorage::ID ThreadID;
		ObjectStorage::ID CurrentMessageID;
		int LineOffset;
		bool AutoScroll;

		MessageDraft Draft;
	};

	ChatState *_chatStack;

	void PushChat(const ObjectStorage::ID &threadID);
	void PopChat();
};

#endif
