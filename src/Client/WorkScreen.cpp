#include "WorkScreen.hpp"

#include <curses.h>

#include "LoginScreen.hpp"
#include "AccountScreen.hpp"
#include "ContactScreen.hpp"
//#include "AttachmentScreen.hpp"
#include "TextColor.hpp"
#include "UiLayout.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/Hex.hpp"
#include "../Common/File.hpp"

enum
{
	LayoutTextBoxHeight = 5
};

WorkScreen::WorkScreen(Root *root)
{
	_root = root;
	_chatStack = nullptr;

	_workAsLineCounter = false;
	_lineCounterValue = 0;

	_h1Y = 2;
	_h2Y = 4;
	_h3Y = 6;
	_h4Y = 8;
	_v1X = 2;
}

WorkScreen::~WorkScreen()
{
	while (_chatStack) {
		ChatState *tmp = _chatStack;
		_chatStack = _chatStack->Next;
		delete tmp;
	}
}

void WorkScreen::Redraw()
{
	RedrawFrames();
	RedrawChatList();

	if (_chatStack) {
		if (_chatStack->Writing) {
			RedrawCurrentChat();
			RedrawTextBox();
		} else {
			RedrawTextBox();
			RedrawCurrentChat();
		}
	}
}

Screen *WorkScreen::ProcessEvent(int event)
{
	if (event == _root->Conf->WorkConnectKey()) {
		if (_root->Conf->GetName().Length()) {
			return new LoginScreen(_root);
		} else {
			_root->Ui->Notify("User name is not specified.");
			return new AccountScreen(_root);
		}
	}

	if (event == _root->Conf->WorkAccountKey()) {
		return new AccountScreen(_root);
	}

	if (event == _root->Conf->WorkContactKey()) {
		return new ContactScreen(_root, this);
	}

	if (event == _root->Conf->WorkExitKey()) {
		return nullptr;
	}

	if (!_chatStack) {
		return ProcessChatListEvent(event);
	}

	return ProcessChatScreenEvent(event);
}

void WorkScreen::ProcessResizeScreen()
{
	if (!_chatStack || _chatStack->CurrentMessageID.IsZero()) {
		return;
	}

	int currentMessageHeight = GetMessageHeight(
		_root->Messages->GetMessageDescriptor(
			_chatStack->CurrentMessageID));

	bool applyLimit = _chatStack->LineOffset > currentMessageHeight - 5;

	if (applyLimit) {
		// Show sender and timestamp.
		_chatStack->LineOffset = currentMessageHeight - 5;

		if (_chatStack->LineOffset < 0) {
			_chatStack->LineOffset = 0;
		}
	}
}

CowBuffer<String> WorkScreen::GetControlHelp()
{
#warning TODO: don't forget to finish control help.
	CowBuffer<String> result(3);
	result[0] = "Exit: " + _root->Conf->WorkExitName();
	result[1] = "Manage contacts: " + _root->Conf->WorkContactName();
	result[2] = "Connect: " + _root->Conf->WorkConnectName();

	return result;
}

void WorkScreen::ActivateCurrentChat()
{
	PushChat(ObjectStorage::ID());
}

void WorkScreen::RedrawFrames()
{
	/*  Login
	 *  PublicKey
	 *  ConnectionStatus
	 *  VoiceStatus
	 *  ----------------------------------  h1
	 *  CurrentContactName, thread
	 *  ----------------------------------  h2
	 *  ContactList | CurrentChatMessages
	 *              |
	 *              |
	 *              |
	 *              |
	 *              |
	 *              |---------------------  h3
	 *              | CurrentChatTextbox
	 *              |
	 *  ----------------------------------  h4
	 *  Help
	 *
	 *              v1
	 */

	_h1Y = LayoutConstants::HeaderHeight;
	_h2Y = _h1Y + 2;

	_h4Y = _rows - 1 - LayoutConstants::FooterHeight;
	_h3Y = _h4Y - LayoutTextBoxHeight - 1;

	_v1X = _columns / 4;

	for (int i = 0; i < _columns; i++) {
		move(_h1Y, i);
		addch(ACS_HLINE);

		move(_h2Y, i);
		addch(ACS_HLINE);

		if (i > _v1X) {
			move(_h3Y, i);
			addch(ACS_HLINE);
		}

		move(_h4Y, i);
		addch(ACS_HLINE);
	}

	for (int i = _h2Y + 1; i < _h4Y; i++) {
		move(i, _v1X);
		addch(ACS_VLINE);
	}

	move(_h2Y, _v1X);
	addch(ACS_TTEE);

	move(_h4Y, _v1X);
	addch(ACS_BTEE);

	move(_h3Y, _v1X);
	addch(ACS_LTEE);

	move(_h2Y + 1, 0);
}

void WorkScreen::RedrawChatList()
{
	int fromY = _h2Y + 1;
	int toY = _h4Y - 1;

	String currentName = _root->Messages->GetCurrentChatName();

	if (!currentName.Length()) {
		move(fromY, 0);
		return;
	}

	String upName = currentName;
	String downName = currentName;

	int height = toY - fromY + 1;
	int size = 1;

	while (size < height) {
		bool grow = false;

		String upperName = _root->Messages->GetPreviousChatName(upName);

		if (upperName.Length()) {
			upName = upperName;
			++size;
			grow = true;
		}

		if (size >= height) {
			break;
		}

		String lowerName = _root->Messages->GetNextChatName(downName);

		if (lowerName.Length()) {
			downName = lowerName;
			++size;
			grow = true;
		}

		if (!grow) {
			break;
		}
	}

	int currentChatPosition = fromY;

	for (int i = fromY; i <= toY; i++) {
		bool thisChatIsCurrent = upName == currentName;

		if (_root->Messages->HasUnread(upName)) {
			attrset(COLOR_PAIR(YELLOW_TEXT));
			move(i, 0);
			addstr("!");
		}

		move(i, 1);

		if (thisChatIsCurrent) {
			addstr("-> ");
		}

		int widthLimit = _v1X - 2;

		if (thisChatIsCurrent) {
			widthLimit -= 3;
		}

		bool running = UiHelpers::DrawRunningLine(
			upName,
			widthLimit);

		if (running) {
			_root->Ui->RequestRunningLine();
		}

		attrset(COLOR_PAIR(DEFAULT_TEXT));

		if (thisChatIsCurrent) {
			currentChatPosition = i;
		}

		upName = _root->Messages->GetNextChatName(upName);

		if (!upName.Length()) {
			break;
		}
	}

	move(currentChatPosition, 0);
}

void WorkScreen::RedrawCurrentChat()
{
	if (!_chatStack) {
		return;
	}

	_root->Messages->SelectOrCreateChat(_chatStack->PeerName);

	move(_h1Y + 1, 0);

	String chatCaptionLine;

	if (_chatStack->ThreadID.IsZero()) {
		chatCaptionLine = "Main thread";
	} else {
		MessageEventProcessor::MessageDescriptorBase *md =
			_root->Messages->GetMessageDescriptor(
				_chatStack->ThreadID);

		if (!md) {
			chatCaptionLine = "Thread " + DataToHex(
				_chatStack->ThreadID.GetValuePointer(),
				(int)ObjectStorage::Constants::IDSize);
		} else {
			chatCaptionLine = "Thread " +
				md->GetHeader().Source + ", " +
				TimeInSecondsToString(
					md->GetHeader().Timestamp);
		}
	}

	bool running = UiHelpers::DrawCommentedLine(
		_chatStack->PeerName,
		chatCaptionLine,
		_columns,
		COLOR_PAIR(DEFAULT_TEXT),
		COLOR_PAIR(DEFAULT_TEXT));

	if (running) {
		_root->Ui->RequestRunningLine();
	}

	ObjectStorage::ID currentMessageID = _chatStack->CurrentMessageID;

	if (_chatStack->AutoScroll) {
		currentMessageID = _root->Messages->GetRootMessageForThread(
			_chatStack->ThreadID);

		_chatStack->CurrentMessageID = currentMessageID;
		_chatStack->LineOffset = 0;
	} else if (!currentMessageID.IsZero()) {
		int messageHeight = GetMessageHeight(
			_root->Messages->GetMessageDescriptor(
				currentMessageID));

		if (_chatStack->LineOffset >= messageHeight) {
			_chatStack->LineOffset = messageHeight - 1;
		}
	}

	int currentLinePosition = _h3Y - 1;
	int skipLines = _chatStack->LineOffset;

	while (currentLinePosition >= _h2Y + 1) {
		if (currentMessageID.IsZero()) {
			RedrawConversationStart(currentLinePosition, skipLines);
			break;
		}

		MessageEventProcessor::MessageDescriptorBase *md =
			_root->Messages->GetMessageDescriptor(currentMessageID);

		bool success = RedrawMessage(
			currentLinePosition,
			skipLines,
			md);

		if (!success) {
			break;
		}

		currentMessageID = _root->Messages->GetPreviousMessage(
			currentMessageID);
	}

	move(_h3Y - 1, _v1X + 1);
}

bool WorkScreen::AddLineToChatScreen(
	int &currentLinePosition,
	int &skipLines,
	int prefix,
	String text,
	bool centering,
	int applyRunFrom)
{
	if (_workAsLineCounter) {
		++_lineCounterValue;
		return true;
	}

	if (currentLinePosition < _h2Y + 1) {
		return false;
	}

	if (skipLines > 0) {
		--skipLines;
		return true;
	}

	if (applyRunFrom != -1) {
		String staticText = text.Substring(0, applyRunFrom);
		String runningText = text.Substring(
			applyRunFrom,
			text.Length() - applyRunFrom);

		int widthLimit =
			_columns -
			_v1X - 3 -
			staticText.Length();

		if (prefix) {
			widthLimit -= 2;
		}

		if (widthLimit <= 0) {
			return AddLineToChatScreen(
				currentLinePosition,
				skipLines,
				prefix,
				text,
				centering,
				0);
		}

		bool isRunning;
		runningText = UiHelpers::GetRunningLine(
			runningText,
			widthLimit,
			isRunning);

		if (isRunning) {
			_root->Ui->RequestRunningLine();
		}

		text = staticText + runningText;
	}

	if (centering) {
		int posX = (_v1X + _columns) / 2 - text.Length() / 2;
		move(currentLinePosition, posX);
	} else {
		move(currentLinePosition, _v1X + 2);
	}

	if (prefix) {
		addch(prefix);
		addch(' ');
	}

	addstr(text.CStr());
	--currentLinePosition;

	return true;
}

int WorkScreen::GetMessageHeight(
	MessageEventProcessor::MessageDescriptorBase *md)
{
	_workAsLineCounter = true;
	_lineCounterValue = 0;

	int lp = 0;
	int sl = 0;

	RedrawMessage(lp, sl, md);

	int result = _lineCounterValue;
	_workAsLineCounter = false;
	_lineCounterValue = 0;
	return result;
}

bool WorkScreen::RedrawConversationStart(
	int &currentLinePosition,
	int &skipLines)
{
	bool success = AddLineToChatScreen(
		currentLinePosition,
		skipLines,
		0,
		"",
		false,
		-1);

	if (!success) {
		return false;
	}

	return AddLineToChatScreen(
		currentLinePosition,
		skipLines,
		0,
		"Conversation start",
		true,
		0);
}

bool WorkScreen::RedrawMessage(
	int &currentLinePosition,
	int &skipLines,
	MessageEventProcessor::MessageDescriptorBase *md)
{
	bool success = RedrawMessageBody(
		currentLinePosition,
		skipLines,
		md);

	if (!success) {
		return false;
	}

	success = RedrawMessageHeader(
		currentLinePosition,
		skipLines,
		md);

	if (!success) {
		return false;
	}

	return RedrawMessageDelimiter(
		currentLinePosition,
		skipLines);
}

bool WorkScreen::RedrawMessageBody(
	int &currentLinePosition,
	int &skipLines,
	MessageEventProcessor::MessageDescriptorBase *md)
{
	if (md->DecryptionFailure()) {
		attrset(COLOR_PAIR(RED_TEXT));
		bool success = AddLineToChatScreen(
			currentLinePosition,
			skipLines,
			0,
			"Corrupt",
			false,
			0);
		attrset(COLOR_PAIR(DEFAULT_TEXT));
		return success;
	}

	if (!md->HasContents() && !md->DecryptionInProgress()) {
		md->RunDecryption();
	}

	if (md->DecryptionInProgress()) {
		attrset(COLOR_PAIR(YELLOW_TEXT));
		bool success = AddLineToChatScreen(
			currentLinePosition,
			skipLines,
			0,
			"Decryption in progress...",
			false,
			0);
		attrset(COLOR_PAIR(DEFAULT_TEXT));
		return success;
	}

	const Message::Contents &contents = md->GetContents();

	for (int i = contents.Entries.Size() - 1; i >= 0; i--) {
		bool success;

		switch (contents.Entries[i]->Type) {
		case Message::ContentsEntryType::Text:
			success = RedrawTextContentsEntry(
				contents.Entries[i],
				currentLinePosition,
				skipLines);
			break;
		case Message::ContentsEntryType::Attachment:
			success = RedrawAttachmentContentsEntry(
				contents.Entries[i],
				currentLinePosition,
				skipLines);
			break;
		default:
			success = RedrawUnknownContentsEntry(
				currentLinePosition,
				skipLines);
			break;
		}

		if (!success) {
			return false;
		}
	}

	return true;
}

bool WorkScreen::RedrawTextContentsEntry(
	Message::ContentsEntry *e,
	int &currentLinePosition,
	int &skipLines)
{
	Message::ContentsEntryText *entry =
		static_cast<Message::ContentsEntryText*>(e);

	CowBuffer<String> lines = UiHelpers::MakeMultiline(
		entry->Text,
		_columns - _v1X - 4);

	for (int i = lines.Size() - 1; i >= 0; i--) {
		bool success = AddLineToChatScreen(
			currentLinePosition,
			skipLines,
			ACS_VLINE,
			lines[i],
			false,
			-1);

		if (!success) {
			return false;
		}
	}

	return true;
}

bool WorkScreen::RedrawAttachmentContentsEntry(
	Message::ContentsEntry *e,
	int &currentLinePosition,
	int &skipLines)
{
	Message::ContentsEntryAttachment *entry =
		static_cast<Message::ContentsEntryAttachment*>(e);

	String line = entry->AttachmentName +
		" [" + DataSizeToString(entry->Attachment.Size()) + "]";

	attrset(COLOR_PAIR(YELLOW_TEXT));
	bool success = AddLineToChatScreen(
		currentLinePosition,
		skipLines,
		ACS_VLINE,
		line,
		false,
		0);
	attrset(COLOR_PAIR(DEFAULT_TEXT));

	return success;
}

bool WorkScreen::RedrawUnknownContentsEntry(
	int &currentLinePosition,
	int &skipLines)
{
	String line = "Unknown entry type.";

	attrset(COLOR_PAIR(RED_TEXT));
	bool success = AddLineToChatScreen(
		currentLinePosition,
		skipLines,
		ACS_VLINE,
		line,
		false,
		0);
	attrset(COLOR_PAIR(DEFAULT_TEXT));

	return success;
}

static void AddFlagToString(String &flags, String flag)
{
	if (!flags.Length()) {
		flags = flag;
	} else {
		flags += ", " + flag.ToLowerCase();
	}
}

bool WorkScreen::RedrawMessageHeader(
	int &currentLinePosition,
	int &skipLines,
	MessageEventProcessor::MessageDescriptorBase *md)
{
	const Message::X25519::HeaderPointToPoint &header = md->GetHeader();
	bool inbound = md->HasAttribute(Message::Attribute::Inbound);

	if (inbound) {
		String warningString;

		ContactStorage *contactStorage =
			_root->Messages->GetContactStorage();

		if (!contactStorage->HasContact(header.Source)) {
			warningString = "Not in contact list.";
		} else {
			Contact *contact = contactStorage->GetContact(
				header.Source);

			if (!contact->IsKeyVerified(header.SourceKey)) {
				warningString = "Key is not verified.";
			}
		}

		if (warningString.Length()) {
			attrset(COLOR_PAIR(RED_TEXT));
			bool success = AddLineToChatScreen(
				currentLinePosition,
				skipLines,
				0,
				warningString,
				false,
				0);
			attrset(COLOR_PAIR(DEFAULT_TEXT));

			if (!success) {
				return false;
			}
		}
	}

	String flagString;

	if (inbound) {
		if (md->HasAttribute(Message::Attribute::Unread)) {
			flagString = "Unread";
		}
	} else {
		if (!md->HasAttribute(Message::Attribute::Unread)) {
			flagString = "Read";
		}

		if (md->HasAttribute(Message::Attribute::Local)) {
			AddFlagToString(flagString, "Local");
		}

		if (md->HasAttribute(Message::Attribute::InProgress)) {
			AddFlagToString(flagString, "Delivery in progress");
		}

		if (md->HasAttribute(Message::Attribute::ConnectionFailure)) {
			AddFlagToString(flagString, "Connection failure");
		}

		if (md->HasAttribute(Message::Attribute::Rejected)) {
			AddFlagToString(flagString, "Rejected");
		}

		if (md->HasAttribute(Message::Attribute::WrongDestinationUser))
		{
			AddFlagToString(flagString, "Invalid destination user");
		}

		if (md->HasAttribute(Message::Attribute::WrongDestinationKey)) {
			AddFlagToString(flagString, "Invalid destination key");
		}

		if (md->HasAttribute(Message::Attribute::InvalidHeader)) {
			AddFlagToString(flagString, "Invalid header");
		}

		if (md->HasAttribute(Message::Attribute::MessageTooBig)) {
			AddFlagToString(flagString, "Message is too big");
		}

		if (md->HasAttribute(Message::Attribute::BannedSender)) {
			AddFlagToString(flagString, "You are banned");
		}

		if (md->HasAttribute(Message::Attribute::BannedSenderKey)) {
			AddFlagToString(flagString, "Your key is banned");
		}

		if (md->HasAttribute(Message::Attribute::Duplicate)) {
			AddFlagToString(flagString, "Duplicate");
		}
	}

	if (flagString.Length()) {
		attrset(COLOR_PAIR(YELLOW_TEXT));
		bool success = AddLineToChatScreen(
			currentLinePosition,
			skipLines,
			0,
			flagString + ".",
			false,
			0);
		attrset(COLOR_PAIR(DEFAULT_TEXT));

		if (!success) {
			return false;
		}
	}

	bool success = AddLineToChatScreen(
		currentLinePosition,
		skipLines,
		0,
		TimeInSecondsToString(header.Timestamp),
		false,
		0);

	if (!success) {
		return false;
	}

	if (inbound) {
		String senderName = header.Source;

		attrset(COLOR_PAIR(YELLOW_TEXT));
		success = AddLineToChatScreen(
			currentLinePosition,
			skipLines,
			0,
			senderName,
			false,
			0);
		attrset(COLOR_PAIR(DEFAULT_TEXT));
	} else {
		attrset(COLOR_PAIR(GREEN_TEXT));
		success = AddLineToChatScreen(
			currentLinePosition,
			skipLines,
			0,
			"You",
			false,
			-1);
		attrset(COLOR_PAIR(DEFAULT_TEXT));
	}

	return success;
}

bool WorkScreen::RedrawMessageDelimiter(
	int &currentLinePosition,
	int &skipLines)
{
	bool success = AddLineToChatScreen(
		currentLinePosition,
		skipLines,
		0,
		"",
		false,
		-1);

	if (!success) {
		return false;
	}

	String text = "-";

	while (text.Length() < _columns - _v1X - 4) {
		text += " -";
	}

	attrset(COLOR_PAIR(YELLOW_TEXT));
	success =  AddLineToChatScreen(
		currentLinePosition,
		skipLines,
		0,
		text,
		false,
		-1);
	attrset(COLOR_PAIR(DEFAULT_TEXT));

	if (!success) {
		return false;
	}

	success = AddLineToChatScreen(
		currentLinePosition,
		skipLines,
		0,
		"",
		false,
		-1);

	return success;
}

void WorkScreen::RedrawTextBox()
{
	MessageDraft &draft = _chatStack->Draft;

	MessageDraft::DraftEntryBase *currItem = draft.GetCurrentEntry();

	if (!currItem || currItem->Type != MessageDraft::EntryType::Text) {
		move(_h3Y + 1, _v1X + 1);
		return;
	}

	MessageDraft::DraftText *e = static_cast<MessageDraft::DraftText*>(
		currItem);

	e->Editor.SetPosition(
		_h3Y + 1,
		_h4Y - 1,
		_v1X + 1,
		_columns - 2);

	e->Editor.Redraw();
}

Screen *WorkScreen::ProcessChatListEvent(int event)
{
	if (event == _root->Conf->WorkListSelectKey()) {
		PushChat(ObjectStorage::ID());
		return this;
	}

	if (event == _root->Conf->WorkListUpKey()) {
		String prevChatName = _root->Messages->GetPreviousChatName(
			_root->Messages->GetCurrentChatName());

		if (prevChatName.Length()) {
			_root->Messages->SelectOrCreateChat(prevChatName);
		}

		return this;
	}

	if (event == _root->Conf->WorkListDownKey()) {
		String nextChatName = _root->Messages->GetNextChatName(
			_root->Messages->GetCurrentChatName());

		if (nextChatName.Length()) {
			_root->Messages->SelectOrCreateChat(nextChatName);
		}

		return this;
	}

	return this;
}

Screen *WorkScreen::ProcessChatScreenEvent(int event)
{
	if (_chatStack && _chatStack->Writing) {
		return ProcessChatTypeEvent(event);
	}

	if (event == _root->Conf->WorkChatBackKey()) {
		PopChat();
		return this;
	}

	if (event == _root->Conf->WorkChatTypeKey()) {
		_chatStack->Writing = true;
		return this;
	}

	if (event == _root->Conf->VoiceStartKey()) {
		Contact *contact = _root->Messages->GetContactStorage()->
			GetContact(_chatStack->PeerName);

		if (!contact) {
			_root->Ui->Notify("Failed to start call. "
				"Peer is not in contacts.");
			return this;
		}

		if (!contact->HasDefaultKey()) {
			_root->Ui->Notify("Failed to start call. "
				"Peer does not have default key.");
			return this;
		}

		_root->Voice->InitCall(
			_chatStack->PeerName,
			contact->GetDefaultKey());

		return this;
	}

	if (event == _root->Conf->WorkChatUpKey()) {
		if (_chatStack->CurrentMessageID.IsZero()) {
			_chatStack->LineOffset = 0;
			_chatStack->AutoScroll = true;
			return this;
		}

		_chatStack->LineOffset += 1;
		_chatStack->AutoScroll = false;

		ObjectStorage::ID prevMessageID =
			_root->Messages->GetPreviousMessage(
				_chatStack->CurrentMessageID);

		bool hasPrevMessage = !prevMessageID.IsZero();

		int currentMessageHeight = GetMessageHeight(
			_root->Messages->GetMessageDescriptor(
				_chatStack->CurrentMessageID));

		bool applyLimit =
			!hasPrevMessage &&
			_chatStack->LineOffset >= currentMessageHeight;

		bool overflowUp =
			hasPrevMessage &&
			_chatStack->LineOffset >= currentMessageHeight;

		if (applyLimit) {
			_chatStack->LineOffset = currentMessageHeight - 1;
		} else if (overflowUp) {
			_chatStack->CurrentMessageID = prevMessageID;
			_chatStack->LineOffset = 0;
		}

		MarkMessageAsRead();

		return this;
	}

	if (event == _root->Conf->WorkChatDownKey()) {
		if (_chatStack->CurrentMessageID.IsZero()) {
			_chatStack->LineOffset = 0;
			_chatStack->AutoScroll = true;
			return this;
		}

		_chatStack->LineOffset -= 1;

		if (_chatStack->LineOffset < 0) {
			ObjectStorage::ID nextMessageID =
				_root->Messages->GetNextMessage(
					_chatStack->CurrentMessageID);

			if (nextMessageID.IsZero()) {
				_chatStack->LineOffset = 0;
				_chatStack->AutoScroll = true;
			} else {
				_chatStack->CurrentMessageID = nextMessageID;

				_chatStack->LineOffset = GetMessageHeight(
					_root->Messages->GetMessageDescriptor(
						nextMessageID)) - 1;
			}
		}

		MarkMessageAsRead();

		return this;
	}

	return this;
}

Screen *WorkScreen::ProcessChatTypeEvent(int event)
{
	if (event == _root->Conf->WorkTypeBackKey()) {
		_chatStack->Writing = false;
		return this;
	}

	// TODO: here must be attachment management.

	MessageDraft &draft = _chatStack->Draft;

	if (event == _root->Conf->WorkTypeSendKey()) {
		_root->Messages->SendMessage(&draft, _chatStack->ThreadID);
		return this;
	}

	MessageDraft::DraftEntryBase *currItem = draft.GetCurrentEntry();

	bool needSetSize = false;

	if (!currItem || currItem->Type != MessageDraft::EntryType::Text) {
		draft.InsertNodeAfterCurrent(MessageDraft::EntryType::Text);
		currItem = draft.GetCurrentEntry();
		needSetSize = true;
	}

	MessageDraft::DraftText *e = static_cast<MessageDraft::DraftText*>(
		currItem);

	if (needSetSize) {
		e->Editor.SetPosition(
			_h3Y + 1,
			_h4Y - 1,
			_v1X + 1,
			_columns - 2);
	}

	if (event == _root->Conf->WorkCursorLeftKey()) {
		e->Editor.GoLeft();
		return this;
	}

	if (event == _root->Conf->WorkCursorRightKey()) {
		e->Editor.GoRight();
		return this;
	}

	if (event == _root->Conf->WorkCursorUpKey()) {
		e->Editor.GoUp();
		return this;
	}

	if (event == _root->Conf->WorkCursorDownKey()) {
		e->Editor.GoDown();
		return this;
	}

	e->Editor.AddChar(event);

	return this;
}

void WorkScreen::MarkMessageAsRead()
{
	if (!_chatStack) {
		return;
	}

	const ObjectStorage::ID &messageID = _chatStack->CurrentMessageID;

	if (messageID.IsZero()) {
		return;
	}

	MessageEventProcessor::MessageDescriptorBase *md =
		_root->Messages->GetMessageDescriptor(messageID);

	if (!md->HasAttribute(Message::Attribute::Inbound)) {
		return;
	}

	if (!md->HasAttribute(Message::Attribute::Unread)) {
		return;
	}

	int messageHeight = GetMessageHeight(md);
	int lineAfterFlags = messageHeight - 7;

	// Mark the message as read on the first message body line.
	bool markAsRead = _chatStack->LineOffset == lineAfterFlags;

	if (!markAsRead) {
		return;
	}

	_root->Network->UpdateMessage(
		_chatStack->PeerName,
		messageID,
		Message::Attribute::Unread,
		false);
}

void WorkScreen::PushChat(const ObjectStorage::ID &threadID)
{
	String currentChatName = _root->Messages->GetCurrentChatName();

	if (!currentChatName.Length()) {
		return;
	}

	ChatState *node = new ChatState;
	node->Next = _chatStack;

	node->PeerName = currentChatName;
	node->ThreadID = threadID;
	node->CurrentMessageID = _root->Messages->GetRootMessageForThread(
		threadID);

	node->LineOffset = 0;
	node->AutoScroll = true;

	node->Writing = false;

	_chatStack = node;
}

void WorkScreen::PopChat()
{
	if (!_chatStack) {
		return;
	}

	ChatState *tmp = _chatStack;
	_chatStack = _chatStack->Next;
	delete tmp;

	if (_chatStack) {
		_root->Messages->SelectOrCreateChat(_chatStack->PeerName);
	}
}
