#include "Chat.hpp"

#include <curses.h>
#include <ctime>

#include "TextColor.hpp"
#include "../Common/UnixTime.hpp"

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
	int64_t *latestReceiveTime) :
	_messageStorage(session->PublicKey),
	_attributeStorage(session->PublicKey)
{
	_session = session;
	_notificationSystem = notificationSystem;
	_latestReceiveTime = latestReceiveTime;

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
		return;
	}

	if (event == KEY_BACKSPACE || event == '\b') {
		if (_draft.Length() > 1) {
			_draft = _draft.Substring(0, _draft.Length() - 1);
		} else if (_draft.Length() == 1) {
			_draft.Clear();
		}

		return;
	}

	if (event == 's' - 'a' + 1) {
		if (!_draft.Length()) {
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

	if (event == KEY_ENTER) {
		event = '\n';
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
		_peerKey,
		timestamp,
		index,
		!crypto_verify32(_peerKey, message.Pointer()));

	if (duplicate) {
		return;
	}

	_messageStorage.AddMessage(message);

	if (*_latestReceiveTime < timestamp) {
		*_latestReceiveTime = timestamp;
	}

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
			int64_t timestamp;
			memcpy(
				&timestamp,
				messages[index].Pointer() + KEY_SIZE * 2,
				sizeof(timestamp));

			if (*_latestReceiveTime < timestamp) {
				*_latestReceiveTime = timestamp;
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

	bool res = _session->SendMessage(message, data);

	if (!res) {
		data->SendInProcess = false;
		_notificationSystem->Notify("Failed to send message.");
	}

	if (_currentMessage) {
		_currentMessage += 1;
	}
}
