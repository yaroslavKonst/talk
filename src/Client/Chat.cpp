#include "Chat.hpp"

#include <curses.h>
#include <ctime>

#include "TextColor.hpp"
#include "../Common/UnixTime.hpp"
#include "../Message/Message.hpp"

CowBuffer<uint8_t> MessageContents::Build() const
{
	CowBuffer<uint8_t> text;
	CowBuffer<uint8_t> data;

	if (Text.Length()) {
		text.Resize(sizeof(int32_t) * 2 + Text.Length());
		*text.SwitchType<int32_t>() = EntryTypeText;
		*text.SwitchType<int32_t>(sizeof(int32_t)) =
			Text.Length();

		memcpy(
			text.Pointer(sizeof(int32_t) * 2),
			Text.CStr(),
			Text.Length());
	}

	if (Attachment.Size()) {
		data.Resize(sizeof(int32_t) * 2 + Attachment.Size());
		*data.SwitchType<int32_t>() = EntryTypeData;
		*data.SwitchType<int32_t>(sizeof(int32_t)) =
			Attachment.Size();

		memcpy(
			data.Pointer(sizeof(int32_t) * 2),
			Attachment.Pointer(),
			Attachment.Size());
	}

	return text.Concat(data);
}

void MessageContents::Parse(const CowBuffer<uint8_t> data)
{
	if (!data.Size()) {
		return;
	}

	unsigned int offset = 0;

	while (offset < data.Size()) {
		int type = ' ';

		if (data.Size() - offset >= sizeof(int32_t)) {
			type = *data.SwitchType<int32_t>(offset);
		}

		if (type == EntryTypeText) {
			int size = *data.SwitchType<int32_t>(
				offset + sizeof(int32_t));

			offset += sizeof(int32_t) * 2;
			Text.Clear();

			for (int i = 0; i < size; i++) {
				Text += data[offset];
				++offset;
			}
		} else if (type == EntryTypeData) {
			int size = *data.SwitchType<int32_t>(
				offset + sizeof(int32_t));

			offset += sizeof(int32_t) * 2;
			Attachment.Resize(size);

			for (int i = 0; i < size; i++) {
				Attachment[i] = data[offset];
				++offset;
			}
		} else {
			Text.Clear();

			while (offset < data.Size()) {
				Text += data[offset];
				++offset;
			}
		}
	}
}

MessageDecryptor::MessageDecryptor(
	const CowBuffer<uint8_t> *message,
	MessageContents *contents)
{
	_message = message;
	_contents = contents;

	EncryptedStream inES;
	EncryptedStream outES;

	Message::Header header;
	bool parseResult = Message::GetHeader(*message, header);

	if (!parseResult) {
		THROW("Invalid message header.");
	}

	bool outgoing = !crypto_verify32(
		header.Source,
		_root->Network->GetPublicKey());

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

	_streamReader.Init(inES);

	_decryptedPart = CowBuffer<uint8_t>(
		message->Size() - Message::HeaderSize);

	_offset = 0;
}

void MessageEncryptor::Run()
{

}

MessageDescriptor::MessageDescriptor(
	MessageStorage *msgStorage,
	AttributeStorage *attrStorage,
	Root *root)
{
	_messageStorage = msgStorage;
	_attributeStorage = attrStorage;
	_root = root;

	Enc = nullptr;
	Dec = nullptr;
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

void MessageDescriptor::ProcessQuant()
{
	if (!Enc && !Dec) {
		return;
	}

	if (Enc) {
		Enc->Run();

		if (Enc->End()) {
			delete Enc;
			_root->Ui->Redraw();
			_messageStorage->AddMessage(Message);
			_root->Network->SendMessage(Message);
			return;
		}
	} else if (Dec) {
		Dec->Run();

		if (Dec->End()) {
			delete Dec;
			_root->Ui->Redraw();
			return;
		}
	}

	_root->Dispatcher->RequestQuant(this);
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

	_attributeStorage->SetAttribute(ID, attrs);
}

Chat::Chat(
	Root *root,
	const uint8_t *peerKey,
	int64_t *latestReceiveTime) :
	_messageStorage(_root->Network->GetPublicKey()),
	_attributeStorage(_root->Network->GetPublicKey())
{
	_root = root;
	_latestReceiveTime = latestReceiveTime;

	_utf8ExpectedSize = 0;

	_peerKey = peerKey;

	LoadMessages();

	_currentMessage = _messages.FindBiggest();
}

Chat::~Chat()
{
	UnloadMessages();
}

bool Chat::HasUnread()
{
	return _attributeStorage.ListUnread().Size();
}

void Chat::SwitchUp()
{
	Tree<MessageContainer>::Entry *prev =
		_messages.Previous(_currentMessage);

	if (prev) {
		_currentMessage = prev;
	}

	MarkReadCurrentMessage();
}

void Chat::SwitchDown()
{
	Tree<MessageContainer>::Entry *next =
		_messages.Next(_currentMessage);

	if (next) {
		_currentMessage = next;
	}

	MarkReadCurrentMessage();
}

void Chat::DeliverMessage(CowBuffer<uint8_t> message)
{
	Message::MessageID header;
	bool parseResult = Message::GetID(message, header);

	if (!parseResult) {
		_root->Ui->Notify(
			"Received message with corrupt header.");
		return;
	}

	bool duplicate = _messageStorage.MessageExists(header);

	if (duplicate) {
		return;
	}

	_messageStorage.AddMessage(message);

	if (*_latestReceiveTime < header.Timestamp) {
		*_latestReceiveTime = header.Timestamp;
	}

	MessageDescriptor *md = new MessageDescriptor(
		&_messageStorage,
		&_attributeStorage,
		_root);
	md->ID = header;
	md->Message = message;

	md->Read = false;
	md->Sent = true;
	md->SetSendFailure(false);
	md->SendInProcess = false;

	md->DecryptedData = DecryptMessage(message);

	if (!_currentMessage) {
		_messages.AddEntry(md);
		_currentMessage = _messages.FindBiggest();
	} else if (!_messages.Next(_currentMessage)) {
		_messages.AddEntry(md);
		_currentMessage = _messages.Next(_currentMessage);
	}
}

void Chat::MarkReadCurrentMessage()
{
	if (!_currentMessage) {
		return;
	}

	if (!_currentMessage->Key.Descriptor->Read) {
		_currentMessage->Key.Descriptor->SetRead(true);
	}
}

bool Chat::HasAttachment()
{
	if (_currentMessage) {
		return _currentMessage->Key.Descriptor->
			DecryptedData.Attachment.Size();
	}

	return false;
}

CowBuffer<uint8_t> Chat::ExtractAttachment()
{
	if (_currentMessage) {
		return _currentMessage->Key.Descriptor->
			DecryptedData.Attachment;
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

void Chat::LoadMessages()
{
	CowBuffer<Message::MessageID> messages =
		_messageStorage.GetMessageRange(
			_peerKey,
			0x8000000000000000,
			0x7fffffffffffffff);

	if (!messages.Size()) {
		return;
	}

	for (unsigned long msgIdx = 0; msgIdx < messages.Size(); msgIdx++) {
		const Message::MessageID &header = messages[msgIdx];

		if (*_latestReceiveTime < header.Timestamp) {
			*_latestReceiveTime = header.Timestamp;
		}

		MessageDescriptor *md =
			new MessageDescriptor(
				&_messageStorage,
				&_attributeStorage,
				_root);

		md->ID = header;

		uint32_t attrs = _attributeStorage.GetAttribute(header);

		md->Read = !(attrs & ATTRIBUTE_READ);
		md->Sent = !(attrs & ATTRIBUTE_SENT);
		md->SendFailure = attrs & ATTRIBUTE_FAILURE;
		md->SendInProcess = false;

		_messages.AddEntry(md);
	}
}

void Chat::UnloadMessages()
{
	_currentMessage = _messages.FindSmallest();

	while (_currentMessage) {
		Tree<MessageContainer>::Entry *entry = _currentMessage;
		_currentMessage = _messages.Next(_currentMessage);
		delete entry->Key.Descriptor;
		_messages.RemoveEntry(entry);
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
