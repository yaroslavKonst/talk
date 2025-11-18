#include "UI.hpp"

#include <unistd.h>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#include "../Protocol/ActiveSession.hpp"
#include "../Common/Hex.hpp"
#include "../Common/UnixTime.hpp"

static const char *voiceMessage = "Voice status: ";

#define DEFAULT_TEXT 0
#define GREEN_TEXT 1
#define YELLOW_TEXT 2
#define RED_TEXT 3

// DEBUG
#include <fcntl.h>

static void DEBUGTTY(String message)
{
	int fd = open("/dev/pts/0", O_WRONLY);
	write(fd, message.CStr(), message.Length());
	write(fd, "\n", 1);
	close(fd);
}

// NotificationSystem.
NotificationSystem::NotificationSystem(NotifyRedrawHandler *handler)
{
	_first = nullptr;
	_last = nullptr;

	_handler = handler;
}

NotificationSystem::~NotificationSystem()
{
	while (_first) {
		Notification *tmp = _first;
		_first = _first->Next;
		delete tmp;
	}

	_last = nullptr;
}

void NotificationSystem::Notify(String message)
{
	Notification *notification = new Notification;
	notification->Next = nullptr;
	notification->Message = message;

	if (!_first) {
		_first = notification;
		_last = notification;
	} else {
		_last->Next = notification;
		_last = notification;
	}

	_handler->NotifyRedraw();
}

void NotificationSystem::Redraw()
{
	if (!_first) {
		return;
	}

	int rows;
	int columns;
	getmaxyx(stdscr, rows, columns);

	int messageSize = _first->Message.Length();
	int frameSize = messageSize + 2;

	if (frameSize < 30) {
		frameSize = 30;
	}

	int baseY = rows / 2 - 4;
	int limitY = rows / 2 + 4;

	int baseX = columns / 2 - frameSize / 2 - 2;
	int limitX = columns / 2 + frameSize / 2 + 3;

	// Clear.
	for (int r = baseY; r < limitY; r++) {
		for (int c = baseX; c < limitX; c++) {
			move(r, c);
			addch(' ');
		}
	}

	// Frame.
	for (int r = baseY + 2; r < limitY - 2; r++) {
		move(r, baseX + 1);
		addch(ACS_VLINE);

		move(r, limitX - 2);
		addch(ACS_VLINE);
	}

	for (int c = baseX + 2; c < limitX - 2; c++) {
		move(baseY + 1, c);
		addch(ACS_HLINE);

		move(limitY - 2, c);
		addch(ACS_HLINE);
	}

	move(baseY + 1, baseX + 1);
	addch(ACS_ULCORNER);
	move(baseY + 1, limitX - 2);
	addch(ACS_URCORNER);
	move(limitY - 2, baseX + 1);
	addch(ACS_LLCORNER);
	move(limitY - 2, limitX - 2);
	addch(ACS_LRCORNER);

	move(baseY + 1, baseX + 2);
	addstr("Notification");

	move(baseY + 3, columns / 2 - messageSize / 2);
	addstr(_first->Message.CStr());

	move(baseY + 5, baseX + 3);
	addstr("Press enter to close.");
}

bool NotificationSystem::ProcessEvent(int event)
{
	if (!_first) {
		return false;
	}

	if (event == KEY_ENTER || event == '\n') {
		Notification *tmp = _first;
		_first = _first->Next;
		delete tmp;

		if (!_first) {
			_last = nullptr;
		}
	}

	return true;
}

// MessageDescriptor.
MessageDescriptor::MessageDescriptor(AttributeStorage *attrStorage)
{
	_attributeStorage = attrStorage;
	Next = nullptr;
}

void MessageDescriptor::SetRead(bool value)
{
	Read = value;
	SaveAttributes();
}

void MessageDescriptor::SetSent(bool value)
{
	Sent = value;
	SaveAttributes();
}

void MessageDescriptor::SetSendFailure(bool value)
{
	SendFailure = value;
	SaveAttributes();
}

void MessageDescriptor::SaveAttributes()
{
	int32_t attrs = 0;

	if (!Read) {
		attrs |= ATTRIBUTE_READ;
	}

	if (!Sent) {
		attrs |= ATTRIBUTE_SENT;
	}

	if (SendFailure) {
		attrs |= ATTRIBUTE_FAILURE;
	}

	_attributeStorage->SetAttribute(Message, attrs);
}

// Chat.
Chat::Chat(
	ClientSession *session,
	const uint8_t *peerKey,
	NotificationSystem *notificationSystem) :
	_messageStorage(session->PublicKey),
	_attributeStorage(session->PublicKey)
{
	_session = session;
	_notificationSystem = notificationSystem;

	_typing = false;

	_last = nullptr;
	_loadedMessages = 0;
	_currentMessage = 0;

	_peerKey = peerKey;

	LoadMessages(30);
}

Chat::~Chat()
{
	UnloadMessages();
}

void Chat::Redraw(int rows, int columns)
{
	_rows = rows;
	_columns = columns;

	RedrawMessageWindow();
	RedrawTextWindow();

	if (!_typing) {
		move(_rows - 9, _columns / 4 + 1);
	}
}

bool Chat::HasUnread()
{
	MessageDescriptor *last = _last;

	while (last) {
		if (!last->Read) {
			return true;
		}

		last = last->Next;
	}

	return false;
}

bool Chat::HasUnsent()
{
	MessageDescriptor *last = _last;

	while (last) {
		if (!last->Sent) {
			return true;
		}

		last = last->Next;
	}

	return false;
}

bool Chat::Typing()
{
	return _typing;
}

void Chat::StartTyping()
{
	_typing = true;
}

void Chat::ProcessTyping(int event)
{
	if (event == '\e') {
		_typing = false;
		Redraw(_rows, _columns);
		return;
	}

	if (event == KEY_BACKSPACE || event == '\b') {
		if (_draft.Length() > 1) {
			_draft = _draft.Substring(0, _draft.Length() - 1);
		} else if (_draft.Length() == 1) {
			_draft.Clear();
		}

		RedrawTextWindow();
		return;
	}

	if (event == 's' - 'a' + 1) {
		if (!_draft.Length()) {
			_notificationSystem->Notify(
				"Empty messages are not allowed.");
			return;
		}

		SendMessage();
		return;
	}

	if (event == KEY_ENTER) {
		event = '\n';
	}

	_draft += event;
	RedrawTextWindow();
};

void Chat::SwitchUp()
{
	++_currentMessage;

	if (_currentMessage >= _loadedMessages) {
		_currentMessage = _loadedMessages - 1;
	}

	if (_currentMessage < 0) {
		_currentMessage = 0;
	}
}

void Chat::SwitchDown()
{
	--_currentMessage;

	if (_currentMessage < 0) {
		_currentMessage = 0;
	}
}

void Chat::DeliverMessage(CowBuffer<uint8_t> message)
{
	const uint32_t headerSize = KEY_SIZE * 2 +
		sizeof(int64_t) + sizeof(int32_t);

	if (message.Size() <= headerSize) {
		_notificationSystem->Notify(
			"Received message with corrupt header.");
		return;
	}

	int64_t timestamp;
	int32_t index;
	memcpy(&timestamp, message.Pointer() + KEY_SIZE * 2, sizeof(timestamp));
	memcpy(
		&index,
		message.Pointer() + KEY_SIZE * 2 + sizeof(timestamp),
		sizeof(index));

	bool duplicate = _messageStorage.MessageExists(
		message.Pointer(),
		timestamp,
		index,
		true);

	if (duplicate) {
		return;
	}

	_messageStorage.AddMessage(message);

	MessageDescriptor *md = new MessageDescriptor(&_attributeStorage);
	md->Message = message;

	md->Read = false;
	md->Sent = true;
	md->SetSendFailure(false);
	md->SendInProcess = false;

	md->Text = DecryptMessage(message);

	md->Next = _last;
	_last = md;

	++_loadedMessages;

	if (_currentMessage) {
		_currentMessage += 1;
	}

	RedrawMessageWindow();
}

void Chat::RedrawMessageWindow()
{
	for (int r = 3; r < _rows - 8; r++) {
		for (int c = _columns / 4 + 1; c < _columns; c++) {
			move(r, c);
			addch(' ');
		}
	}

	int drawBase = _rows - 9;
	int drawLimit = 2;

	int messageIndex = 0;

	if (_currentMessage) {
		move(_rows - 10, _columns - 1);
		addch('|');
		move(_rows - 9, _columns - 1);
		addch(ACS_DARROW);
	} else {
		move(_rows - 10, _columns - 1);
		addch(' ');
		move(_rows - 9, _columns - 1);
		addch(' ');
	}

	MessageDescriptor *last = _last;

	while (last) {
		if (messageIndex < _currentMessage) {
			if (!last->Next) {
				LoadMessages(_loadedMessages + 10);
			}

			last = last->Next;
			++messageIndex;
			continue;
		}

		CowBuffer<String> lines(1);

		if (last->Text.Length() == 0) {
			lines[0] = "Corrupt";
			attrset(COLOR_PAIR(RED_TEXT));
		} else {
			lines = MakeMultiline(
				last->Text,
				_columns - _columns / 4 - 4);
		}

		for (int line = lines.Size() - 1; line >= 0; line--) {
			if (drawBase <= drawLimit) {
				attrset(COLOR_PAIR(DEFAULT_TEXT));
				move(3, _columns - 1);
				addch(ACS_UARROW);
				move(4, _columns - 1);
				addch('|');
				return;
			}

			move(drawBase, _columns / 4 + 3);
			addstr(lines[line].CStr());
			--drawBase;
		}

		attrset(COLOR_PAIR(DEFAULT_TEXT));

		if (drawBase <= drawLimit) {
			move(3, _columns - 1);
			addch(ACS_UARROW);
			move(4, _columns - 1);
			addch('|');
			return;
		}

		int64_t timestamp;
		memcpy(
			&timestamp,
			last->Message.Pointer() + KEY_SIZE * 2,
			sizeof(timestamp));
		String timeStr = ctime(&timestamp);
		timeStr = timeStr.Substring(0, timeStr.Length() - 1);

		move(drawBase, _columns / 4 + 2);
		addstr(timeStr.CStr());

		if (!last->Read) {
			attrset(COLOR_PAIR(YELLOW_TEXT));
			addstr(" unread");
			attrset(COLOR_PAIR(DEFAULT_TEXT));
		}

		if (!last->Sent) {
			attrset(COLOR_PAIR(YELLOW_TEXT));
			addstr(" unsent");
			attrset(COLOR_PAIR(DEFAULT_TEXT));
		}

		if (last->SendFailure) {
			attrset(COLOR_PAIR(RED_TEXT));
			addstr(" failure");
			attrset(COLOR_PAIR(DEFAULT_TEXT));
		}

		if (last->SendInProcess) {
			attrset(COLOR_PAIR(GREEN_TEXT));
			addstr(" in process");
			attrset(COLOR_PAIR(DEFAULT_TEXT));
		}

		--drawBase;

		if (drawBase <= drawLimit) {
			move(3, _columns - 1);
			addch(ACS_UARROW);
			move(4, _columns - 1);
			addch('|');
			return;
		}

		bool outgoing = !crypto_verify32(
			last->Message.Pointer(),
			_session->PublicKey);

		move(drawBase, _columns / 4 + 2);

		if (outgoing) {
			addstr("You");
		} else {
			addstr("User");
		}

		drawBase -= 2;

		if (!last->Next) {
			LoadMessages(_loadedMessages + 10);
		}

		last = last->Next;
		++messageIndex;
	}

	if (drawBase <= drawLimit) {
		move(3, _columns - 1);
		addch(ACS_UARROW);
		move(4, _columns - 1);
		addch('|');
		return;
	}

	String startText = "Start of conversation.";

	move(drawBase, _columns * 5 / 8 - startText.Length() / 2);
	addstr(startText.CStr());
}

void Chat::RedrawTextWindow()
{
	for (int r = _rows - 7; r < _rows - 2; r++) {
		for (int c = _columns / 4 + 1; c < _columns; c++) {
			move(r, c);
			addch(' ');
		}
	}

	move(_rows - 7, _columns / 4 + 1);

	if (!_draft.Length()) {
		return;
	}

	CowBuffer<String> lines = MakeMultiline(
		_draft,
		_columns - _columns / 4 - 1);

	if (lines.Size() <= 5) {
		for (uint32_t line = 0; line < lines.Size(); line++) {
			move(_rows - 7 + line, _columns / 4 + 1);
			addstr(lines[line].CStr());
		}

		return;
	}

	int position = _rows - 4;

	for (int line = lines.Size() - 2; line >= 0; line--) {
		if (position < _rows - 7) {
			break;
		}

		move(position, _columns / 4 + 1);
		addstr(lines[line].CStr());
		--position;
	}

	move(_rows - 3, _columns / 4 + 1);
	addstr(lines[lines.Size() - 1].CStr());
}

static bool IsSpace(char c)
{
	return c == ' ' || c == '\n';
}

CowBuffer<String> Chat::MakeMultiline(String text, int limit)
{
	struct Line
	{
		Line *Next;
		String Line;
	};

	Line *first = nullptr;
	Line **last = &first;

	int lineCount = 0;
	bool endWithNewLine = false;

	while (text.Length() > 0) {
		String line;
		bool hasNewLine = false;

		endWithNewLine = false;

		for (int i = 0; i < text.Length(); i++) {
			if (text.CStr()[i] == '\n') {
				hasNewLine = true;
				break;
			}
		}

		if (!hasNewLine && text.Length() < limit) {
			line = text;
			text.Clear();
		} else {
			int position = 0;

			while (position < limit - 1) {
				if (text.CStr()[position] == '\n') {
					endWithNewLine = true;
					break;
				}

				++position;
			}

			while (position >= 0) {
				if (IsSpace(text.CStr()[position])) {
					break;
				}

				--position;
			}

			if (position < 0) {
				position = limit - 1;
			}

			if (position == 0) {
				text = text.Substring(
					1,
					text.Length() - 1);
			} else if (!IsSpace(text.CStr()[position])) {
				line = text.Substring(0, position);
				text = text.Substring(
					position,
					text.Length() - position);
			} else {
				line = text.Substring(0, position);
				text = text.Substring(
					position + 1,
					text.Length() - position - 1);
			}
		}

		*last = new Line;
		(*last)->Next = nullptr;
		(*last)->Line = line;

		last = &((*last)->Next);
		++lineCount;
	}

	if (endWithNewLine) {
		*last = new Line;
		(*last)->Next = nullptr;
		++lineCount;
	}

	CowBuffer<String> result(lineCount);

	int index = 0;

	while (first) {
		result[index] = first->Line;
		++index;

		Line *tmp = first;
		first = first->Next;
		delete tmp;
	}

	return result;
}

void Chat::LoadMessages(int count)
{
	if (count <= _loadedMessages) {
		return;
	}

	MessageDescriptor **last = &_last;

	CowBuffer<CowBuffer<uint8_t>> messages =
		_messageStorage.GetLatestNMessages(_peerKey, count);

	if (!messages.Size()) {
		return;
	}

	int index = 0;

	while (_loadedMessages < (int64_t)messages.Size()) {
		if (!*last) {
			*last = new MessageDescriptor(&_attributeStorage);
			(*last)->Next = nullptr;
			(*last)->Message = messages[index];

			uint32_t attrs = _attributeStorage.GetAttribute(
				messages[index]);

			(*last)->Read = !(attrs & ATTRIBUTE_READ);
			(*last)->Sent = !(attrs & ATTRIBUTE_SENT);
			(*last)->SendFailure = attrs & ATTRIBUTE_FAILURE;
			(*last)->SendInProcess = false;

			(*last)->Text = DecryptMessage((*last)->Message);

			++_loadedMessages;
		}

		last = &((*last)->Next);
		++index;
	}
}

void Chat::UnloadMessages()
{
	_loadedMessages = 0;
	_currentMessage = 0;

	while (_last) {
		MessageDescriptor *tmp = _last;
		_last = _last->Next;

		tmp->Text.Wipe();
		delete tmp;
	}
}

CowBuffer<uint8_t> Chat::EncryptMessage(
	String text,
	const uint8_t *senderKey,
	const uint8_t *receiverKey,
	int64_t timestamp,
	int32_t index)
{
	CowBuffer<uint8_t> header(KEY_SIZE * 2 +
		sizeof(int64_t) + sizeof(int32_t));

	memcpy(header.Pointer(), senderKey, KEY_SIZE);
	memcpy(header.Pointer() + KEY_SIZE, receiverKey, KEY_SIZE);
	memcpy(header.Pointer() + KEY_SIZE * 2, &timestamp, sizeof(timestamp));
	memcpy(
		header.Pointer() + KEY_SIZE * 2 + sizeof(timestamp),
		&index,
		sizeof(index));

	EncryptedStream outES;
	EncryptedStream inES;

	GenerateSessionKeys(
		_session->PrivateKey,
		_session->PublicKey,
		_peerKey,
		timestamp,
		outES.Key,
		inES.Key);

	InitNonce(outES.Nonce);

	CowBuffer<uint8_t> textBuffer(text.Length());

	memcpy(textBuffer.Pointer(), text.CStr(), text.Length());

	CowBuffer<uint8_t> encryptedMessage = Encrypt(
		textBuffer,
		outES,
		header.Pointer(),
		header.Size());

	CowBuffer<uint8_t> result(header.Size() + encryptedMessage.Size());

	memcpy(result.Pointer(), header.Pointer(), header.Size());
	memcpy(
		result.Pointer() + header.Size(),
		encryptedMessage.Pointer(),
		encryptedMessage.Size());

	return result;
}

String Chat::DecryptMessage(CowBuffer<uint8_t> message)
{
	uint32_t headerSize = KEY_SIZE * 2 + sizeof(int64_t) + sizeof(int32_t);

	if (message.Size() <= headerSize) {
		return String();
	}

	CowBuffer<uint8_t> header = message.Slice(0, headerSize);
	CowBuffer<uint8_t> encryptedMessage = message.Slice(
		headerSize,
		message.Size() - headerSize);

	int64_t timestamp;

	memcpy(&timestamp, header.Pointer() + KEY_SIZE * 2, sizeof(timestamp));

	bool outgoing = !crypto_verify32(header.Pointer(), _session->PublicKey);

	EncryptedStream outES;
	EncryptedStream inES;

	GenerateSessionKeys(
		_session->PrivateKey,
		_session->PublicKey,
		_peerKey,
		timestamp,
		inES.Key,
		outES.Key,
		!outgoing);

	memset(inES.Nonce, 0, NONCE_SIZE);

	CowBuffer<uint8_t> decryptedMessage = Decrypt(
		encryptedMessage,
		inES,
		header.Pointer(),
		header.Size());

	String result;

	for (uint64_t i = 0; i < decryptedMessage.Size(); i++) {
		result += decryptedMessage[i];
	}

	return result;
}

void Chat::SendMessage()
{
	int64_t timestamp = GetUnixTime();
	int32_t index;

	_messageStorage.GetFreeTimestampIndex(_peerKey, timestamp, index);

	CowBuffer<uint8_t> message = EncryptMessage(
		_draft,
		_session->PublicKey,
		_peerKey,
		timestamp,
		index);

	_messageStorage.AddMessage(message);

	_messageStorage.AddMessage(message);
	MessageDescriptor *data = new MessageDescriptor(&_attributeStorage);
	data->Message = message;
	data->Read = true;
	data->Sent = false;
	data->SetSendFailure(false);
	data->SendInProcess = true;
	data->Text = _draft;

	data->Next = _last;
	_last = data;

	++_loadedMessages;

	_draft.Wipe();

	RedrawMessageWindow();
	RedrawTextWindow();

	bool res = _session->SendMessage(message, data);

	if (!res) {
		data->SendInProcess = false;
		_notificationSystem->Notify("Failed to send message.");
	}

	if (_currentMessage) {
		_currentMessage += 1;
	}
}

// Chat list.
ChatList::ChatList(
	ClientSession *session,
	NotificationSystem *notificationSystem) :
	_contactList(session->PublicKey)
{
	_session = session;
	_notificationSystem = notificationSystem;

	_chatCount = _contactList.GetContactCount();
	_chatList = nullptr;
	_currentChat = 0;

	if (_chatCount) {
		_chatList = new Chat*[_chatCount];

		for (int i = 0; i < _chatCount; i++) {
			_chatList[i] = new Chat(
				_session,
				_contactList.GetContactKey(i),
				_notificationSystem);
		}
	}
}

ChatList::~ChatList()
{
	if (_chatList) {
		for (int i = 0; i < _chatCount; i++) {
			delete _chatList[i];
		}

		delete[] _chatList;
		_chatList = nullptr;
	}
}

void ChatList::Redraw(int rows, int columns)
{
	_rows = rows;
	_columns = columns;

	int colLimit = _columns / 4;
	int rowBase = 3;
	int rowLimit = _rows - 2;

	int start;
	int end;

	if (_chatCount <= rowLimit - rowBase) {
		start = 0;
		end = _chatCount;
	} else {
		start = _currentChat - (rowLimit - rowBase) / 2;

		if (start < 0) {
			start = 0;
		}

		end = start + (rowLimit - rowBase);

		if (end > _chatCount) {
			end = _chatCount;
		}
	}

	for (int i = start; i < end; i++) {
		String name = _contactList.GetNameForPresentation(i);

		bool hasUnread = _chatList[i]->HasUnread();
		bool hasUnsent = _chatList[i]->HasUnsent();

		if (hasUnread || hasUnsent) {
			name = String("!") + name;
		} else {
			name = String(" ") + name;
		}

		if (name.Length() > colLimit) {
			name = name.Substring(0, colLimit);
		}

		int attrs = 0;

		if (hasUnread || hasUnsent) {
			attrs |= COLOR_PAIR(YELLOW_TEXT);
		}

		if (i == _currentChat) {
			attrs |= A_BOLD;
		}

		if (attrs) {
			attrset(attrs);
		}

		move(rowBase + i - start, 0);
		addstr(name.CStr());
		attrset(COLOR_PAIR(DEFAULT_TEXT));
	}

	move(rowBase + _currentChat - start, 0);
	refresh();
}

Chat *ChatList::GetCurrentChat()
{
	if (!_chatCount) {
		return nullptr;
	}

	return _chatList[_currentChat];
}

void ChatList::SwitchUp()
{
	--_currentChat;

	if (_currentChat < 0) {
		_currentChat = 0;
	}
}

void ChatList::SwitchDown()
{
	++_currentChat;

	if (_currentChat >= _chatCount) {
		_currentChat = _chatCount - 1;
	}

	if (_currentChat < 0) {
		_currentChat = 0;
	}
}

void ChatList::UpdateUserData(const uint8_t *key, String name)
{
	_contactList.AddContact(key, name);

	if (_contactList.GetContactCount() != _chatCount) {
		Chat **list = new Chat*[_chatCount + 1];

		if (_chatList) {
			for (int i = 0; i < _chatCount; i++) {
				list[i] = _chatList[i];
			}

			delete[] _chatList;
		}

		_chatList = list;
		++_chatCount;

		_chatList[_chatCount - 1] = new Chat(
			_session,
			_contactList.GetContactKey(_chatCount - 1),
			_notificationSystem);
	}
}

void ChatList::DeliverMessage(CowBuffer<uint8_t> message)
{
	const uint8_t *senderKey = message.Pointer();

	for (int i = 0; i < _chatCount; i++) {
		if (!crypto_verify32(senderKey, _chatList[i]->GetPeerKey())) {
			_chatList[i]->DeliverMessage(message);
			return;
		}
	}

	UpdateUserData(senderKey, "");
	_chatList[_chatCount - 1]->DeliverMessage(message);
}

// Base screen.
Screen::Screen(ClientSession *session)
{
	_session = session;
	getmaxyx(stdscr, _rows, _columns);
}

Screen::~Screen()
{
}

void Screen::ProcessResize()
{
	getmaxyx(stdscr, _rows, _columns);
	Redraw();
}

void Screen::ClearScreen()
{
	for (int r = 0; r < _rows; r++) {
		for (int c = 0; c < _columns; c++) {
			move(r, c);
			addch(' ');
		}
	}
}

// Password screen.
PasswordScreen::PasswordScreen(ClientSession *session) : Screen(session)
{
}

void PasswordScreen::Redraw()
{
	ClearScreen();

	move(0, 0);
	addstr("Help: Exit: END | Mute: Ctrl-M | Mark read: Ctrl-R");

	move(_rows / 2, 4);
	addstr("Enter password: ");
	addstr(_password.CStr());
	refresh();
}

Screen *PasswordScreen::ProcessEvent(int event)
{
	if (event == KEY_END) {
		return nullptr;
	}

	if (event == KEY_ENTER || event == '\n') {
		if (_password.Length() == 0) {
			int y;
			int x;
			getyx(stdscr, y, x);
			move(_rows / 2 + 2, 10);
			addstr("Password must not be empty.");
			move(y, x);
			return this;
		}

		// Transition to connection.
		int y;
		int x;
		getyx(stdscr, y, x);
		move(_rows / 2 + 2, 10);
		addstr("Processing...              ");
		move(y, x);
		refresh();

		GenerateKeys();

		return new WorkScreen(_session);
	}

	if (event == KEY_BACKSPACE || event == '\b') {
		if (_password.Length() == 0) {
			return this;
		}

		addch('\b');
		addch(' ');
		addch('\b');

		_password = _password.Substring(0, _password.Length() - 1);
		return this;
	}

	if (_password.Length() > 50) {
		int y;
		int x;
		getyx(stdscr, y, x);
		move(_rows / 2 + 2, 10);
		addstr("Password is too long.      ");
		move(y, x);
		return this;
	}

	_password += event;
	addch(event);

	return this;
}

void PasswordScreen::GenerateKeys()
{
	uint8_t salt[SALT_SIZE];
	GetSalt("talk.p.salt", salt);
	DeriveKey(_password.CStr(), salt, _session->PrivateKey);
	crypto_wipe(salt, SALT_SIZE);
	GeneratePublicKey(_session->PrivateKey, _session->PublicKey);

	GetSalt("talk.s.salt", salt);
	uint8_t seed[KEY_SIZE];
	DeriveKey(_password.CStr(), salt, seed);
	GenerateSignature(
		seed,
		_session->SignaturePrivateKey,
		_session->SignaturePublicKey);
	crypto_wipe(salt, SALT_SIZE);

	_password.Wipe();
}

// Login screen.
static const char *ipMessage = "Enter IP address: ";
static const char *portMessage = "Enter port: ";

LoginScreen::LoginScreen(ClientSession *session) : Screen(session)
{
	_writingIp = true;
	_writingPort = false;
	_writingKey = false;
}

void LoginScreen::Redraw()
{
	ClearScreen();

	move(0, 0);
	addstr("Help: Exit: END | Mute: Ctrl-M | Mark read: Ctrl-R");

	move(_rows / 2 - 10, 4);
	addstr("Your public key:");
	move(_rows / 2 - 9, 4);
	String hex = DataToHex(_session->PublicKey, KEY_SIZE);
	addstr(hex.CStr());

	move(_rows / 2 - 7, 4);
	addstr("Your signature:");
	move(_rows / 2 - 6, 4);
	hex = DataToHex(_session->SignaturePublicKey, KEY_SIZE);
	addstr(hex.CStr());

	move(_rows / 2 - 4, 4);
	addstr(ipMessage);
	addstr(_ip.CStr());

	move(_rows / 2 - 2, 4);
	addstr(portMessage);
	addstr(_port.CStr());

	move(_rows / 2, 4);
	addstr("Server public key:");
	move(_rows / 2 + 1, 4);
	addstr(_serverKeyHex.CStr());

	if (_writingIp) {
		move(
			_rows / 2 - 4,
			4 + strlen(ipMessage) + strlen(_ip.CStr()));
	} else if (_writingPort) {
		move(
			_rows / 2 - 2,
			4 + strlen(portMessage) + strlen(_port.CStr()));
	} else if (_writingKey) {
		move(
			_rows / 2 + 1,
			4 + strlen(_serverKeyHex.CStr()));
	}

	refresh();
}

Screen *LoginScreen::ProcessEvent(int event)
{
	if (event == KEY_END) {
		return nullptr;
	}

	if (event == KEY_ENTER || event == '\n') {
		if (_writingIp) {
			_writingIp = false;
			_writingPort = true;
			move(
				_rows / 2 - 2,
				4 + strlen(portMessage) + strlen(_port.CStr()));
			return this;
		} else if (_writingPort) {
			_writingPort = false;
			_writingKey = true;
			move(
				_rows / 2 + 1,
				4 + strlen(_serverKeyHex.CStr()));
			return this;
		}

		// Transition to work state.
		if (_serverKeyHex.Length() != KEY_SIZE * 2) {
			int y;
			int x;
			getyx(stdscr, y, x);
			move(_rows / 2 + 3, 10);
			addstr("Invalid server key length.");
			move(y, x);
			return this;
		}

		HexToData(_serverKeyHex, _session->PeerPublicKey);

		{
			int y;
			int x;
			getyx(stdscr, y, x);
			move(_rows / 2 + 3, 10);
			addstr("Connecting...");
			move(y, x);
		}

		uint16_t port = atoi(_port.CStr());

		if (port == 0) {
			int y;
			int x;
			getyx(stdscr, y, x);
			move(_rows / 2 + 3, 10);
			addstr("Invalid port number.");
			move(y, x);
			return this;
		}

		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		int res = inet_aton(_ip.CStr(), &addr.sin_addr);

		if (!res) {
			int y;
			int x;
			getyx(stdscr, y, x);
			move(_rows / 2 + 3, 10);
			addstr("Invalid IP address.");
			move(y, x);
			return this;
		}

		_session->Socket = socket(AF_INET, SOCK_STREAM, 0);

		if (_session->Socket == -1) {
			int y;
			int x;
			getyx(stdscr, y, x);
			move(_rows / 2 + 3, 10);
			addstr("Failed to create socket.");
			move(y, x);
			return this;
		}

		res = connect(
			_session->Socket,
			(struct sockaddr*)&addr,
			sizeof(addr));

		if (res == -1) {
			close(_session->Socket);
			_session->Socket = -1;
			int y;
			int x;
			getyx(stdscr, y, x);
			move(_rows / 2 + 3, 10);
			addstr("Failed to connect.");
			move(y, x);
			return this;
		}

		bool initRes = _session->InitSession();

		if (!initRes) {
			close(_session->Socket);
			_session->Socket = -1;
			int y;
			int x;
			getyx(stdscr, y, x);
			move(_rows / 2 + 3, 10);
			addstr("Failed to init session.");
			move(y, x);
			return this;
		}

		return nullptr;
	}

	if (event == KEY_BACKSPACE || event == '\b') {
		if (_writingIp) {
			if (_ip.Length() == 0) {
				return this;
			}

			_ip = _ip.Substring(0, _ip.Length() - 1);
		} else if (_writingPort) {
			if (_port.Length() == 0) {
				_writingPort = false;
				_writingIp = true;
				move(
					_rows / 2 - 4,
					4 + strlen(ipMessage) +
					strlen(_ip.CStr()));
				return this;
			}

			_port = _port.Substring(0, _port.Length() - 1);
		} else if (_writingKey) {
			if (_serverKeyHex.Length() == 0) {
				_writingKey = false;
				_writingPort = true;
				move(
					_rows / 2 - 2,
					4 + strlen(portMessage) +
					strlen(_port.CStr()));
				return this;
			}

			_serverKeyHex = _serverKeyHex.Substring(
				0,
				_serverKeyHex.Length() - 1);
		}

		addch('\b');
		addch(' ');
		addch('\b');
		return this;
	}

	if (_writingIp) {
		if ((event < '0' || event > '9') && event != '.') {
			return this;
		}

		_ip += event;
	} else if (_writingPort) {
		if (event < '0' || event > '9') {
			return this;
		}

		_port += event;
	} else if (_writingKey) {
		_serverKeyHex += event;
	}

	addch(event);
	return this;
}

// Work screen.
WorkScreen::WorkScreen(ClientSession *session) :
	Screen(session),
	_notificationSystem(this),
	_chatList(session, &_notificationSystem)
{
	_session->Processor = this;
	_overlay = nullptr;
	_activeChat = nullptr;
}

WorkScreen::~WorkScreen()
{
	_session->Processor = nullptr;

	if (_overlay) {
		delete _overlay;
		_overlay = nullptr;
	}
}

void WorkScreen::Redraw()
{
	if (_overlay) {
		_overlay->ProcessResize();
		return;
	}

	ClearScreen();

	move(0, 0);
	addstr("Help: Exit: END | Mute: Ctrl-M | Mark read: Ctrl-R");

	move(1, 0);
	addstr("Connection status: ");

	if (_session->Connected()) {
		attrset(COLOR_PAIR(GREEN_TEXT));
		addstr("connected");
	} else {
		attrset(COLOR_PAIR(YELLOW_TEXT));
		addstr("not connected");
	}

	attrset(COLOR_PAIR(DEFAULT_TEXT));
	addstr(".");

	move(2, _columns / 4);
	addch(ACS_TTEE);

	move(_rows - 2, _columns / 4);
	addch(ACS_BTEE);

	for (int i = 3; i < _rows - 2; i++) {
		move(i, _columns / 4);

		if (i != _rows - 8) {
			addch(ACS_VLINE);
		} else {
			addch(ACS_LTEE);
		}
	}

	for (int i = 0; i < _columns; i++) {
		if (i == _columns / 4) {
			continue;
		}

		move(2, i);
		addch(ACS_HLINE);

		move(_rows - 2, i);
		addch(ACS_HLINE);

		if (i > _columns / 4) {
			move(_rows - 8, i);
			addch(ACS_HLINE);
		}
	}

	move(_rows - 1, 0);
	addstr(voiceMessage);
	addstr("not connected.");

	_chatList.Redraw(_rows, _columns);

	if (_activeChat) {
		_activeChat->Redraw(_rows, _columns);
	}

	_notificationSystem.Redraw();

	refresh();
}

Screen *WorkScreen::ProcessEvent(int event)
{
	bool hasNotification = _notificationSystem.ProcessEvent(event);

	if (hasNotification) {
		Redraw();
		return this;
	}

	if (event == KEY_HOME) {
		Connect();
		return this;
	}

	if (_overlay) {
		Screen *overlay = _overlay->ProcessEvent(event);
		refresh();

		if (!overlay) {
			delete _overlay;
			_overlay = nullptr;
			Redraw();
		}

		return this;
	}

	if (event == KEY_END) {
		return nullptr;
	}

	if (event == 'n' - 'a' + 1) {
		static int count = 1;

		String text;

		for (int i = 0; i < count; i++) {
			text += "a";
		}

		++count;

		_notificationSystem.Notify(text);
	}

	if (!_activeChat) {
		ProcessChatListEvent(event);
	} else if (!_activeChat->Typing()) {
		ProcessChatScreenEvent(event);
	} else {
		_activeChat->ProcessTyping(event);
	}

	return this;
}

void WorkScreen::NotifyDelivery(void *userPointer, int32_t status)
{
	MessageDescriptor *message = static_cast<MessageDescriptor*>(
		userPointer);

	message->SendInProcess = false;

	if (status == SESSION_RESPONSE_OK) {
		message->SetSent(true);
		Redraw();
		return;
	}

	bool hardFault =
		status == SESSION_RESPONSE_ERROR_INVALID_USER ||
		status == SESSION_RESPONSE_ERROR_MESSAGE_TOO_SHORT;

	if (hardFault) {
		message->SetSendFailure(true);
		message->SetSent(true);
	}

	if (status == SESSION_RESPONSE_ERROR_INVALID_USER) {
		_notificationSystem.Notify(
			"Sent message contains invalid user.");
	} else if (status == SESSION_RESPONSE_ERROR_MESSAGE_TOO_SHORT) {
		_notificationSystem.Notify("Sent message has invalid format.");
	} else if (status == SESSION_RESPONSE_ERROR_CONNECTION_LOST) {
		_notificationSystem.Notify(
			"Connection lost. Message was not sent.");
	} else {
		_notificationSystem.Notify(
			"Unknown error code on sent message.");
	}

	Redraw();
}

void WorkScreen::DeliverMessage(CowBuffer<uint8_t> header)
{
	_chatList.DeliverMessage(header);
	Redraw();
}

void WorkScreen::UpdateUserData(const uint8_t *key, String name)
{
	_chatList.UpdateUserData(key, name);
	Redraw();
}

void WorkScreen::Connect()
{
	if (_session->Connected() || _overlay) {
		return;
	}

	_overlay = new LoginScreen(_session);
	_overlay->Redraw();
}

void WorkScreen::ProcessChatListEvent(int event)
{
	if (event == 'u' - 'a' + 1) {
		// Ctrl-U.
		_session->RequestUserList();
	} else if (event == KEY_UP) {
		_chatList.SwitchUp();
		_chatList.Redraw(_rows, _columns);
	} else if (event == KEY_DOWN) {
		_chatList.SwitchDown();
		_chatList.Redraw(_rows, _columns);
	} else if (event == KEY_ENTER || event == '\n') {
		_activeChat = _chatList.GetCurrentChat();
		_activeChat->Redraw(_rows, _columns);
	}
}

void WorkScreen::ProcessChatScreenEvent(int event)
{
	if (event == '\e') {
		_activeChat = nullptr;
		Redraw();
	} else if (event == KEY_ENTER || event == '\n') {
		_activeChat->StartTyping();
		_activeChat->Redraw(_rows, _columns);
	} else if (event == KEY_UP) {
		_activeChat->SwitchUp();
		_activeChat->Redraw(_rows, _columns);
	} else if (event == KEY_DOWN) {
		_activeChat->SwitchDown();
		_activeChat->Redraw(_rows, _columns);
	}
}

// UI main.
UI::UI(ClientSession *session)
{
	initscr();
	raw();
	noecho();
	keypad(stdscr, 1);
	start_color();

	init_pair(GREEN_TEXT, COLOR_GREEN, COLOR_BLACK);
	init_pair(YELLOW_TEXT, COLOR_YELLOW, COLOR_BLACK);
	init_pair(RED_TEXT, COLOR_RED, COLOR_BLACK);

	_session = session;
	_screen = new PasswordScreen(_session);

	ProcessResize();
}

UI::~UI()
{
	endwin();
}

void UI::ProcessResize()
{
	if (_screen) {
		_screen->ProcessResize();
	}
}

bool UI::ProcessEvent()
{
	int event = getch();

	if (event == KEY_RESIZE) {
		ProcessResize();
		return true;
	}

	Screen *newScreen = _screen->ProcessEvent(event);
	refresh();

	if (newScreen == _screen) {
		return true;
	}

	delete _screen;
	_screen = newScreen;

	if (_screen) {
		_screen->Redraw();
	}

	return _screen;
}

void UI::Disconnect()
{
	if (!_session->Connected()) {
		return;
	}

	_session->Close();
	_session->State = ClientSession::ClientStateUnconnected;

	while (_session->SMUserPointersFirst) {
		ClientSession::SMUser *tmp = _session->SMUserPointersFirst;
		_session->SMUserPointersFirst =
			_session->SMUserPointersFirst->Next;

		_session->Processor->NotifyDelivery(
			tmp->Pointer,
			SESSION_RESPONSE_ERROR_CONNECTION_LOST);
		delete tmp;
	}

	_session->SMUserPointersLast = nullptr;

	if (_screen) {
		_screen->Redraw();
	}
}
