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
	void ProcessResizeScreen() override;

	CowBuffer<String> GetControlHelp() override;

	void ActivateCurrentChat();

private:
	Root *_root;

	void RedrawFrames();
	void RedrawChatList();

	// Layout. Scheme is in cpp file.
	int _h1Y;
	int _h2Y;
	int _h3Y;
	int _h4Y;
	int _v1X;

	void RedrawCurrentChat();

	// Chat screen line drawing helper.
	bool AddLineToChatScreen(
		int &currentLinePosition,
		int &skipLines,
		int prefix,
		String text,
		bool centering,
		int applyRunFrom);
	bool _workAsLineCounter;
	int _lineCounterValue;
	int GetMessageHeight(
		MessageEventProcessor::MessageDescriptorBase *md);

	// Chat screen draw helpers.
	bool RedrawConversationStart(int &currentLinePosition, int &skipLines);
	bool RedrawMessage(
		int &currentLinePosition,
		int &skipLines,
		MessageEventProcessor::MessageDescriptorBase *md);
	bool RedrawMessageBody(
		int &currentLinePosition,
		int &skipLines,
		MessageEventProcessor::MessageDescriptorBase *md);
	bool RedrawTextContentsEntry(
		Message::ContentsEntry *e,
		int &currentLinePosition,
		int &skipLines);
	bool RedrawAttachmentContentsEntry(
		Message::ContentsEntry *e,
		int &currentLinePosition,
		int &skipLines);
	bool RedrawUnknownContentsEntry(
		int &currentLinePosition,
		int &skipLines);
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
	Screen *ProcessChatTypeEvent(int event);

	void MarkMessageAsRead();

	struct ChatState
	{
		ChatState *Next;

		String PeerName;
		ObjectStorage::ID ThreadID;
		ObjectStorage::ID CurrentMessageID;
		int LineOffset;
		bool AutoScroll;

		MessageDraft Draft;

		bool Writing;
	};

	ChatState *_chatStack;

	void PushChat(const ObjectStorage::ID &threadID);
	void PopChat();
};

#endif
