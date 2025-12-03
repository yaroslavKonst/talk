#include "Chat.hpp"

#include <curses.h>
#include <ctime>

#include "TextColor.hpp"
#include "../Common/UnixTime.hpp"
#include "../Message/Message.hpp"

static bool IsSpace(char c)
{
	return c == ' ' || c == '\n';
}

static bool IsUTF8Trailing(char c)
{
	return (c & 0xc0) == 0x80;
}

static int StrLenUTF8(const char *str)
{
	int length = 0;

	while (*str) {
		if (!IsUTF8Trailing(*str)) {
			++length;
		}

		++str;
	}

	return length;
}

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

Chat::Chat(
	ClientSession *session,
	const uint8_t *peerKey,
	NotificationSystem *notificationSystem,
	int64_t *latestReceiveTime,
	ControlStorage *controls) :
	_messageStorage(session->PublicKey),
	_attributeStorage(session->PublicKey)
{
	_session = session;
	_notificationSystem = notificationSystem;
	_latestReceiveTime = latestReceiveTime;
	_controls = controls;

	_typing = false;
	_utf8ExpectedSize = 0;

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
	if (event == _controls->WorkChatBackKey()) {
		_typing = false;
		return;
	}

	if (event == _controls->WorkTypeSendKey()) {
		String text = _draft + _draftSuffix;

		if (!text.Length() && !_draftAttachment.Size()) {
			_notificationSystem->Notify(
				"Empty messages are not allowed.");
			return;
		}

		if (!_session->Connected()) {
			_notificationSystem->Notify(
				"No connection. Unable to send message.");
			return;
		}

		SendMessage();
		return;
	}

	if (event == '\b') {
		if (_draft.Length() > 1) {
			while (_draft.Length() > 1 && IsUTF8Trailing(
				_draft.CStr()[_draft.Length() - 1]))
			{
				_draft = _draft.Substring(
					0,
					_draft.Length() - 1);
			}

			_draft = _draft.Substring(0, _draft.Length() - 1);
		} else if (_draft.Length() == 1) {
			_draft.Clear();
		}

		return;
	}

	if (event == _controls->WorkCursorLeftKey()) {
		if (!_draft.Length()) {
			return;
		}

		while (IsUTF8Trailing(_draft.CStr()[_draft.Length() - 1])) {
			_draftSuffix =
				_draft.Substring(_draft.Length() - 1, 1) +
				_draftSuffix;
			_draft = _draft.Substring(0, _draft.Length() - 1);
		}

		_draftSuffix =
			_draft.Substring(_draft.Length() - 1, 1) +
			_draftSuffix;
		_draft = _draft.Substring(0, _draft.Length() - 1);
		return;
	}

	if (event == _controls->WorkCursorRightKey()) {
		if (!_draftSuffix.Length()) {
			return;
		}

		_draft += _draftSuffix.CStr()[0];
		_draftSuffix =
			_draftSuffix.Substring(1, _draftSuffix.Length() - 1);

		while (IsUTF8Trailing(_draftSuffix.CStr()[0])) {
			_draft += _draftSuffix.CStr()[0];
			_draftSuffix = _draftSuffix.Substring(
				1,
				_draftSuffix.Length() - 1);
		}

		return;
	}

	if (event == KEY_DC) {
		if (!_draftSuffix.Length()) {
			return;
		}

		_draftSuffix =
			_draftSuffix.Substring(1, _draftSuffix.Length() - 1);

		while (IsUTF8Trailing(_draftSuffix.CStr()[0])) {
			_draftSuffix = _draftSuffix.Substring(
				1,
				_draftSuffix.Length() - 1);
		}

		return;
	}

	if (event != '\n' && (event > 0xff || event < ' ')) {
		return;
	}

	if ((event & 0x80) || _utf8ExpectedSize) {
		if (!_utf8ExpectedSize) {
			if ((event & 0xc0) == 0x80) {
				_notificationSystem->Notify(
					"Lonely UTF-8 trailing byte.");
				return;
			}

			if ((event & 0xe0) == 0xc0) {
				_utf8ExpectedSize = 2;
			} else if ((event & 0xf8) == 0xe0) {
				_utf8ExpectedSize = 3;
			} else if ((event & 0xf8) == 0xf0) {
				_utf8ExpectedSize = 4;
			} else {
				_notificationSystem->Notify(
					"Invalid first UTF-8 byte.");
				return;
			}

			_utf8Buffer += event;
		} else {
			if ((event & 0xc0) != 0x80) {
				_notificationSystem->Notify(
					"UTF-8 trailing byte expected.");
				_utf8ExpectedSize = 0;
				_utf8Buffer.Clear();
				return;
			}

			_utf8Buffer += event;

			if (_utf8ExpectedSize == _utf8Buffer.Length()) {
				_draft += _utf8Buffer;
				_utf8ExpectedSize = 0;
				_utf8Buffer.Wipe();
			}
		}

		return;
	}

	_draft += event;
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

	MarkRead(_currentMessage);
}

void Chat::SwitchDown()
{
	--_currentMessage;

	if (_currentMessage < 0) {
		_currentMessage = 0;
	}

	MarkRead(_currentMessage);
}

void Chat::DeliverMessage(CowBuffer<uint8_t> message)
{
	Message::Header header;
	bool parseResult = Message::GetHeader(message, header);

	if (!parseResult) {
		_notificationSystem->Notify(
			"Received message with corrupt header.");
		return;
	}

	bool duplicate = _messageStorage.MessageExists(
		_peerKey,
		header.Timestamp,
		header.Index,
		!crypto_verify32(_peerKey, header.Source));

	if (duplicate) {
		return;
	}

	_messageStorage.AddMessage(message);

	if (*_latestReceiveTime < header.Timestamp) {
		*_latestReceiveTime = header.Timestamp;
	}

	MessageDescriptor *md = new MessageDescriptor(&_attributeStorage);
	md->Message = message;

	md->Read = false;
	md->Sent = true;
	md->SetSendFailure(false);
	md->SendInProcess = false;

	md->DecryptedData = DecryptMessage(message);

	md->Next = _last;
	_last = md;

	++_loadedMessages;

	if (_currentMessage) {
		_currentMessage += 1;
	}
}

void Chat::MarkRead()
{
	MessageDescriptor *md = _last;

	while (md) {
		if (!md->Read) {
			md->SetRead(true);
		}

		md = md->Next;
	}
}

void Chat::MarkRead(int messageIndex)
{
	MessageDescriptor *last = _last;
	int currentIndex = 0;

	while (last) {
		if (currentIndex == messageIndex) {
			if (!last->Read) {
				last->SetRead(true);
			}

			break;
		}

		last = last->Next;
		++currentIndex;
	}
}

bool Chat::HasAttachment()
{
	MessageDescriptor *last = _last;
	int currentIndex = 0;

	while (last) {
		if (currentIndex == _currentMessage) {
			return last->DecryptedData.Attachment.Size();
		}

		last = last->Next;
		++currentIndex;
	}

	return false;
}

CowBuffer<uint8_t> Chat::ExtractAttachment()
{
	MessageDescriptor *last = _last;
	int currentIndex = 0;

	while (last) {
		if (currentIndex == _currentMessage) {
			return last->DecryptedData.Attachment;
		}

		last = last->Next;
		++currentIndex;
	}

	return CowBuffer<uint8_t>();
}

void Chat::AddAttachment(const CowBuffer<uint8_t> attachment)
{
	_draftAttachment = attachment;
}

void Chat::ClearAttachment()
{
	_draftAttachment.Wipe();
}

void Chat::RedrawMessageWindow()
{
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

	String peerName = _peerName;

	if (peerName.Length() > _columns * 3 / 4 - 3) {
		peerName = peerName.Substring(0, _columns * 3 / 4 - 3);
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

		if (last->DecryptedData.IsEmpty()) {
			lines[0] = "Corrupt";
			attrset(COLOR_PAIR(RED_TEXT));
		} else if (last->DecryptedData.Text.Length()) {
			lines = MakeMultiline(
				last->DecryptedData.Text,
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

		if (last->DecryptedData.Attachment.Size()) {
			int attachSize = last->DecryptedData.Attachment.Size();
			move(drawBase, _columns / 4 + 2);
			attrset(COLOR_PAIR(YELLOW_TEXT));
			addstr("Attachment ");

			if (attachSize < 1024 * 10) {
				addstr(ToString(attachSize).CStr());
				addstr(" B");
			} else if (attachSize < 1024 * 1024 * 10) {
				addstr(ToString(attachSize / 1024).CStr());
				addstr(" KB");
			} else {
				addstr(ToString(
					attachSize / 1024 / 1024).CStr());
				addstr(" MB");
			}

			addstr((". " +
				_controls->WorkChatExtractName() +
				" to extract.").CStr());
			attrset(COLOR_PAIR(DEFAULT_TEXT));
			--drawBase;

			if (drawBase <= drawLimit) {
				move(3, _columns - 1);
				addch(ACS_UARROW);
				move(4, _columns - 1);
				addch('|');
				return;
			}
		}

		Message::Header header;
		int res = Message::GetHeader(last->Message, header);

		if (!res) {
			THROW("Invalid message header.");
		}

		String timeStr = ctime(&header.Timestamp);
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
			header.Source,
			_session->PublicKey);

		move(drawBase, _columns / 4 + 2);

		if (outgoing) {
			attrset(COLOR_PAIR(GREEN_TEXT));
			addstr("You");
			attrset(COLOR_PAIR(DEFAULT_TEXT));
		} else {
			attrset(COLOR_PAIR(YELLOW_TEXT));
			addstr(peerName.CStr());
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

		attrset(COLOR_PAIR(YELLOW_TEXT));

		for (int c = _columns / 4 + 2; c < _columns - 1; c += 2) {
			move(drawBase, c);
			addch('-');
		}

		attrset(COLOR_PAIR(DEFAULT_TEXT));

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
	if (_draftAttachment.Size()) {
		move(_rows - 3, _columns / 4 + 1);
		attrset(COLOR_PAIR(YELLOW_TEXT));
		addstr("Attachment ");

		if (_draftAttachment.Size() < 1024 * 10) {
			addstr(ToString(_draftAttachment.Size()).CStr());
			addstr(" B");
		} else if (_draftAttachment.Size() < 1024 * 1024 * 10) {
			addstr(ToString(_draftAttachment.Size() / 1024).CStr());
			addstr(" KB");
		} else {
			addstr(ToString(
				_draftAttachment.Size() / 1024 / 1024).CStr());
			addstr(" MB");
		}

		attrset(COLOR_PAIR(DEFAULT_TEXT));
	}

	move(_rows - 7, _columns / 4 + 1);

	String text = _draft + _draftSuffix;

	if (!text.Length()) {
		return;
	}

	int cursorPosition = StrLenUTF8(_draft.CStr());

	CowBuffer<String> lines = MakeMultiline(
		text,
		_columns - _columns / 4 - 1);

	int cursorLine = 0;
	int cursorOffset = 0;

	while (cursorPosition) {
		int lineLength = StrLenUTF8(lines[cursorLine].CStr());

		if (lineLength < cursorPosition) {
			cursorPosition -= lineLength;
			--cursorPosition;
			++cursorLine;
			continue;
		}

		cursorOffset = cursorPosition;
		cursorPosition = 0;
	}

	int windowHeight = 5;

	int startLine = cursorLine - windowHeight / 2;
	int endLine = cursorLine + windowHeight / 2;

	if (_draftAttachment.Size()) {
		--endLine;
	}

	if (startLine < 0) {
		endLine += -startLine;
		startLine = 0;
	}

	if (endLine >= (int)lines.Size()) {
		startLine -= endLine - (lines.Size() - 1);
		endLine = lines.Size() - 1;
	}

	if (startLine < 0) {
		startLine = 0;
	}

	int windowPos = _rows - 7;

	for (int line = startLine; line <= endLine; line++) {
		move(windowPos, _columns / 4 + 1);
		addstr(lines[line].CStr());
		++windowPos;
	}

	move(
		_rows - 7 + cursorLine - startLine,
		_columns / 4 + 1 + cursorOffset);
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

		if (!hasNewLine && StrLenUTF8(text.CStr()) < limit) {
			line = text;
			text.Clear();
		} else {
			int position = 0;
			int charIndex = 0;

			while (charIndex < limit - 1) {
				char currentChar = text.CStr()[position];

				if (currentChar == '\n') {
					endWithNewLine = true;
					break;
				}

				++position;

				if (!IsUTF8Trailing(currentChar)) {
					++charIndex;
				}
			}

			while (position >= 0) {
				char currentChar = text.CStr()[position];

				if (IsSpace(currentChar)) {
					break;
				}

				--position;

				if (!IsUTF8Trailing(currentChar)) {
					--charIndex;
				}
			}

			if (position < 0) {
				charIndex = 0;
				position = 0;

				while (charIndex < limit - 1)
				{
					if (position >= text.Length() - 1) {
						break;
					}

					char currentChar =
						text.CStr()[position];

					++position;

					if (!IsUTF8Trailing(currentChar)) {
						++charIndex;
					}
				}

				while (IsUTF8Trailing(text.CStr()[position])) {
					++position;
				}
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
			Message::Header header;
			bool res = Message::GetHeader(messages[index], header);

			if (!res) {
				THROW("Invalid message header.");
			}

			if (*_latestReceiveTime < header.Timestamp) {
				*_latestReceiveTime = header.Timestamp;
			}

			*last = new MessageDescriptor(&_attributeStorage);
			(*last)->Next = nullptr;
			(*last)->Message = messages[index];

			uint32_t attrs = _attributeStorage.GetAttribute(
				messages[index]);

			(*last)->Read = !(attrs & ATTRIBUTE_READ);
			(*last)->Sent = !(attrs & ATTRIBUTE_SENT);
			(*last)->SendFailure = attrs & ATTRIBUTE_FAILURE;
			(*last)->SendInProcess = false;

			(*last)->DecryptedData =
				DecryptMessage((*last)->Message);

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

		tmp->DecryptedData.Text.Wipe();
		tmp->DecryptedData.Attachment.Wipe();
		delete tmp;
	}
}

CowBuffer<uint8_t> Chat::EncryptMessage(
	const MessageContents messageContents,
	const uint8_t *senderKey,
	const uint8_t *receiverKey,
	int64_t timestamp,
	int32_t index)
{
	Message::Header header;
	header.Source = senderKey;
	header.Destination = receiverKey;
	header.Timestamp = timestamp;
	header.Index = index;

	CowBuffer<uint8_t> headerBuffer = Message::BuildHeader(header);

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

	CowBuffer<uint8_t> textBuffer = messageContents.Build();

	const CowBuffer<uint8_t> encryptedMessage = Encrypt(
		textBuffer,
		outES,
		headerBuffer.Pointer(),
		headerBuffer.Size());

	return Message::BuildMessage(headerBuffer, encryptedMessage);
}

MessageContents Chat::DecryptMessage(const CowBuffer<uint8_t> message)
{
	Message::Header header;
	bool parseResult = Message::GetHeader(message, header);

	if (!parseResult) {
		return MessageContents();
	}

	const CowBuffer<uint8_t> encryptedMessage = message.Slice(
		Message::HeaderSize,
		message.Size() - Message::HeaderSize);

	bool outgoing = !crypto_verify32(header.Source, _session->PublicKey);

	EncryptedStream outES;
	EncryptedStream inES;

	GenerateSessionKeys(
		_session->PrivateKey,
		_session->PublicKey,
		_peerKey,
		header.Timestamp,
		inES.Key,
		outES.Key,
		!outgoing);

	memset(inES.Nonce, 0, NONCE_SIZE);

	const CowBuffer<uint8_t> decryptedMessage = Decrypt(
		encryptedMessage,
		inES,
		message.Pointer(),
		Message::HeaderSize);

	MessageContents contents;
	contents.Parse(decryptedMessage);
	return contents;
}

void Chat::SendMessage()
{
	MessageContents contents;
	contents.Text = _draft + _draftSuffix;
	contents.Attachment = _draftAttachment;

	int64_t timestamp = GetUnixTime();
	int32_t index;

	_messageStorage.GetFreeTimestampIndex(_peerKey, timestamp, index);

	CowBuffer<uint8_t> message = EncryptMessage(
		contents,
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
	data->DecryptedData = contents;

	data->Next = _last;
	_last = data;

	++_loadedMessages;

	_draft.Wipe();
	_draftSuffix.Wipe();
	_draftAttachment.Wipe();

	bool res = _session->SendMessage(message, data);

	if (!res) {
		data->SendInProcess = false;
		_notificationSystem->Notify("Failed to send message.");
	}

	if (_currentMessage) {
		_currentMessage += 1;
	}
}
