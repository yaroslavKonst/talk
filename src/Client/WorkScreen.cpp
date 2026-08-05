#include "WorkScreen.hpp"

#include <curses.h>

#include "LoginScreen.hpp"
#include "ContactScreen.hpp"
//#include "AttachmentScreen.hpp"
#include "TextColor.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/Hex.hpp"
#include "../Common/File.hpp"

WorkScreen::WorkScreen(Root *root)
{
	_root = root;
	_chatStack = nullptr;
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
		return new LoginScreen(_root);
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
	 *  ----------------------------------
	 *  CurrentContactName
	 *  ----------------------------------
	 *  ContactList | CurrentChatMessages
	 *              |
	 *              |
	 *              |
	 *              |
	 *              |
	 *              |---------------------
	 *              | CurrentChatTextbox
	 *              |
	 *  ----------------------------------
	 *  Help
	 */

	const int h1Y = 4;
	const int h2Y = 6;
	const int h3Y = _rows - 9;
	const int h4Y = _rows - 3;

	const int v1X = _columns / 4;

	for (int i = 0; i < _columns; i++) {
		move(h1Y, i);
		addch(ACS_HLINE);

		move(h2Y, i);
		addch(ACS_HLINE);

		if (i > v1X) {
			move(h3Y, i);
			addch(ACS_HLINE);
		}

		move(h4Y, i);
		addch(ACS_HLINE);
	}

	for (int i = h2Y + 1; i < h4Y; i++) {
		move(i, v1X);
		addch(ACS_VLINE);
	}

	move(h2Y, v1X);
	addch(ACS_TTEE);

	move(h4Y, v1X);
	addch(ACS_BTEE);

	move(h3Y, v1X);
	addch(ACS_LTEE);

	move(5, 0);
}

void WorkScreen::RedrawChatList()
{
	int fromY = 7;
	int toY = _columns - 4;

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
		if (_root->Messages->HasUnread(upName)) {
			attrset(COLOR_PAIR(YELLOW_TEXT));
			move(i, 0);
			addstr("!");
		}

		move(i, 1);
		addstr(upName.CStr());
		attrset(COLOR_PAIR(DEFAULT_TEXT));

		if (upName == currentName) {
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

	move(5, 0);

	addstr(_chatStack->PeerName.CStr());

	_root->Messages->SelectOrCreateChat(_chatStack->PeerName);

	int currentLinePosition = _rows - 10;
	int skipLines = _chatStack->LineOffset;

	ObjectStorage::ID currentMessageID = _chatStack->CurrentMessageID;

	if (_chatStack->AutoScroll) {
		for (;;) {
			ObjectStorage::ID nextID =
				_root->Messages->GetNextMessage(
					currentMessageID);

			if (nextID.IsZero()) {
				break;
			}

			currentMessageID = nextID;
		}
	}

	while (currentLinePosition >= 7) {
		if (currentMessageID.IsZero()) {
			RedrawConversationStart(currentLinePosition, skipLines);
			break;
		}

		MessageEventProcessor::MessageDescriptorBase *md =
			_root->Messages->GetMessageDescriptor(currentMessageID);

		bool success = RedrawMessageBody(
			currentLinePosition,
			skipLines,
			md);

		if (!success) {
			break;
		}

		success = RedrawMessageHeader(
			currentLinePosition,
			skipLines,
			md);

		if (!success) {
			break;
		}

		success = RedrawMessageDelimiter(
			currentLinePosition,
			skipLines);

		if (!success) {
			break;
		}

		currentMessageID = _root->Messages->GetPreviousMessage(
			currentMessageID);
	}

	move(_rows - 10, _columns / 4 + 1);
}

bool WorkScreen::AddLineToChatScreen(
	int &currentLinePosition,
	int &skipLines,
	String text,
	bool centering)
{
	if (currentLinePosition < 7) {
		return false;
	}

	if (skipLines > 0) {
		--skipLines;
		return true;
	}

	if (centering) {
		int posX = _columns * 5 / 8 - text.Length() / 2;
		move(currentLinePosition, posX);
	} else {
		move(currentLinePosition, _columns / 4 + 2);
	}

	addstr(text.CStr());
	--currentLinePosition;

	return true;
}

bool WorkScreen::RedrawConversationStart(
	int &currentLinePosition,
	int &skipLines)
{
	bool success = AddLineToChatScreen(currentLinePosition, skipLines, "");

	if (!success) {
		return false;
	}

	return AddLineToChatScreen(
		currentLinePosition,
		skipLines,
		"Conversation start",
		true);
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
			"Corrupt");
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
			"Decryption in progress...");
		attrset(COLOR_PAIR(DEFAULT_TEXT));
		return success;
	}

#warning TODO: replace this stub.
	return AddLineToChatScreen(
		currentLinePosition,
		skipLines,
		"Message body placeholder.");
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
			AddFlagToString(flagString, "In progress");
		}

		if (md->HasAttribute(Message::Attribute::ConnectionFailure)) {
			AddFlagToString(flagString, "Connection failure");
		}

		if (md->HasAttribute(Message::Attribute::MessageTooBig)) {
			AddFlagToString(flagString, "Message is too big");
		}

		if (md->HasAttribute(Message::Attribute::BannedUser)) {
			AddFlagToString(flagString, "You are banned");
		}

		if (md->HasAttribute(Message::Attribute::BannedKey)) {
			AddFlagToString(flagString, "Your key is banned");
		}
	}

	if (flagString.Length()) {
		bool success = AddLineToChatScreen(
			currentLinePosition,
			skipLines,
			flagString + ".");

		if (!success) {
			return false;
		}
	}

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
				warningString);
			attrset(COLOR_PAIR(DEFAULT_TEXT));

			if (!success) {
				return false;
			}
		}
	}

	bool success = AddLineToChatScreen(
		currentLinePosition,
		skipLines,
		TimeInSecondsToString(header.Timestamp));

	if (!success) {
		return false;
	}

	if (inbound) {
		String senderName = header.Source;

		attrset(COLOR_PAIR(YELLOW_TEXT));
		success = AddLineToChatScreen(
			currentLinePosition,
			skipLines,
			senderName);
		attrset(COLOR_PAIR(DEFAULT_TEXT));
	} else {
		attrset(COLOR_PAIR(GREEN_TEXT));
		success = AddLineToChatScreen(
			currentLinePosition,
			skipLines,
			"You");
		attrset(COLOR_PAIR(DEFAULT_TEXT));
	}

	return success;
}

bool WorkScreen::RedrawMessageDelimiter(
	int &currentLinePosition,
	int &skipLines)
{
	bool success = AddLineToChatScreen(currentLinePosition, skipLines, "");

	if (!success) {
		return false;
	}

	String text = "-";

	while (text.Length() < _columns * 3 / 4 - 4) {
		text += " -";
	}

	attrset(COLOR_PAIR(YELLOW_TEXT));
	success =  AddLineToChatScreen(
		currentLinePosition,
		skipLines,
		text);
	attrset(COLOR_PAIR(DEFAULT_TEXT));

	return success;
}

void WorkScreen::RedrawTextBox()
{
	MessageDraft &draft = _chatStack->Draft;

	MessageDraft::DraftEntryBase *currItem = draft.GetCurrentEntry();

	if (!currItem || currItem->Type != MessageDraft::EntryType::Text) {
		move(_rows - 8, _columns / 4 + 1);
		return;
	}

	MessageDraft::DraftText *e = static_cast<MessageDraft::DraftText*>(
		currItem);

	e->Editor.SetPosition(
		_rows - 8,
		_rows - 4,
		_columns / 4 + 1,
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
		_root->Messages->SendMessage(&draft);
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
			_rows - 8,
			_rows - 4,
			_columns / 4 + 1,
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
