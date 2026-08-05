#include "Chat.hpp"

#include "../Message/Message.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/Hex.hpp"
#include "../Common/File.hpp"

Chat::Chat(
	Root *root,
	String peerName) :
	_objectStorage(
		"storage/" + DataToHex(
			root->PublicKey->Key,
			Crypto::X25519::KEY_SIZE) +
		"/storage/" + peerName,
		root->Dispatcher)
{
	_root = root;
	_peerName = peerName;

	_currentMessage = nullptr;
	_currentMessageLine = 0;
	_enc = nullptr;
	_encLock = nullptr;

	_draftPtr = nullptr;

	LoadMessages();
}

Chat::~Chat()
{
	_root->Dispatcher->UnregisterQuantProcessor(this);

	if (_encLock) {
		_root->Ui->BlockCancel(_encLock);
		_encLock = nullptr;
	}

	if (_enc) {
		delete _enc;
		_enc = nullptr;
	}

	UnloadMessages();
}

bool Chat::HasMessage(const ObjectStorage::ID &messageID)
{
	return _messagesByID.FindEntry(messageID);
}

bool Chat::HasUnread()
{
	return _unreadMessages.FindSmallest();
}

void Chat::SendMessage(MessageDraft *draft)
{
	if (draft->IsEmpty()) {
		_root->Ui->Notify("Can't send empty message.");
		return;
	}

	if (!_root->Network->ConnectionActive()) {
		_root->Ui->Notify("No connection to server.");
		return;
	}

	draft->PushState();

	while (draft->SwitchToNextEntry()) { }

	uint64_t count = 1;

	while (draft->SwitchToPreviousEntry()) {
		count += 1;
	}

	Message::Contents contents;
	contents.Entries.Resize(count);

	uint64_t index = 0;

	do {
		MessageDraft::DraftEntryBase *node = draft->GetCurrentEntry();

		if (node->Type == MessageDraft::EntryType::Text) {
			MessageDraft::DraftText *draftText =
				static_cast<MessageDraft::DraftText*>(node);

			Message::ContentsEntryText *entry =
				new Message::ContentsEntryText();
			entry->Type = Message::ContentsEntryType::Text;
			entry->Text = draftText->Editor.GetText();

			contents.Entries[index] = entry;
		} else if (node->Type == MessageDraft::EntryType::Attachment) {
			MessageDraft::DraftAttachment *draftAttachment =
				static_cast<MessageDraft::DraftAttachment*>(
					node);

			Message::ContentsEntryAttachment *entry =
				new Message::ContentsEntryAttachment();
			entry->Type = Message::ContentsEntryType::Attachment;
			entry->AttachmentName = draftAttachment->Name;
			entry->Attachment = draftAttachment->Data;

			contents.Entries[index] = entry;
		}

		index += 1;
	} while (draft->SwitchToNextEntry());

	draft->PopState();

	ContactStorage *contactStorage = _root->Messages->GetContactStorage();

	if (!contactStorage->HasContact(_peerName)) {
		_root->Ui->Notify("Peer is not in contact list.");
		return;
	}

	Contact *contact = contactStorage->GetContact(_peerName);

	if (!contact->HasDefaultKey()) {
		_root->Ui->Notify("This contact does not have default key.");
		return;
	}

	Message::X25519::HeaderPointToPoint header;
	header.Source =
		_root->Conf->GetName() + "@" + _root->Conf->GetHostName();
	header.SourceKey = *_root->PublicKey;
	header.Destination = _peerName;
	header.DestinationKey = contact->GetDefaultKey();
	header.ThreadID = _currentThreadID;
	header.Timestamp = GetUnixTime();

	_enc = new MessageEncryptor(
		_root,
		&_objectStorage,
		_peerName,
		&contents,
		header,
		&_encMessage);

	if (_enc->Failure()) {
		_root->Ui->Notify("Message encryption failed.");
		return;
	}

	_encLock = _root->Ui->BlockNotify("Encrypting message...");
	_draftPtr = draft;

	_root->Dispatcher->RegisterQuantProcessor(this);
}

void Chat::DeliverMessage(
	const CowBuffer<uint8_t> message,
	Message::Attribute attr,
	bool local)
{
	Message::X25519::HeaderPointToPoint header;

	bool parseResult = Message::X25519::ParseHeader(message, header);

	if (!parseResult) {
		return;
	}

	ObjectStorage::ID messageID;
	messageID.SetValue(Crypto::GetHash(
		message.Slice(0, header.HeaderSize),
		(int)ObjectStorage::Constants::IDSize).Pointer());

	bool duplicate = _objectStorage.HasObject(messageID);

	if (duplicate) {
		return;
	}

	bool inbound = _peerName == header.Source;

	Message::Attribute attrs = attr;

	if (inbound) {
		attrs = Message::AttributeAction::Set(
			attrs,
			Message::Attribute::Inbound);
	} else {
		attrs = Message::AttributeAction::Clear(
			attrs,
			Message::Attribute::Inbound);
	}

	if (local) {
		attrs = Message::AttributeAction::Set(
			attrs,
			Message::Attribute::Local);
		attrs = Message::AttributeAction::Set(
			attrs,
			Message::Attribute::Unread);
	}

	AddMessageToThread(messageID, header, message, attrs);

	_root->Ui->Redraw();
}

void Chat::UpdateMessage(
	const ObjectStorage::ID &messageID,
	Message::Attribute attr,
	bool value)
{
	Tree<MessageTreeEntry>::Entry *entry =
		_messagesByID.FindEntry(messageID);

	if (!entry) {
		return;
	}

	if (value) {
		entry->Key.Node->Descriptor->SetAttribute(attr);
	} else {
		entry->Key.Node->Descriptor->ClearAttribute(attr);
	}

	if (attr == Message::Attribute::Unread) {
		Tree<UnreadMessageTreeEntry>::Entry *e =
			_unreadMessages.FindEntry(
				UnreadMessageTreeEntry(
					messageID,
					entry->Key.Node->Descriptor->
						Header.Timestamp,
					entry->Key.Node->Descriptor->
						Header.Index));

		if (value) {
			if (e) {
				return;
			}

			_unreadMessages.AddEntry(entry->Key.Node);
		} else {
			if (!e) {
				return;
			}

			_unreadMessages.RemoveEntry(e);
		}
	}

	_root->Ui->Redraw();
}

void Chat::ProcessQuant()
{
	if (!_enc) {
		if (_encLock) {
			_root->Ui->BlockCancel(_encLock);
		}

		_encLock = nullptr;
		_draftPtr = nullptr;
		return;
	}

	bool success = _enc->Run();

	if (!success || _enc->Failure()) {
		if (_encLock) {
			_root->Ui->BlockCancel(_encLock);
		}

		_encLock = nullptr;
		_draftPtr = nullptr;
		return;
	}

	if (!_enc->Ended()) {
		_root->Dispatcher->RegisterQuantProcessor(this);
		return;
	}

	if (_encLock) {
		_root->Ui->BlockCancel(_encLock);
	}

	_encLock = nullptr;

	delete _enc;
	_enc = nullptr;

	DeliverMessage(_encMessage, (Message::Attribute)0, true);
	success = _root->Network->SendMessage(_encMessage);

	if (!success) {
		_root->Ui->Notify("Failed to send message.");
	}

	_encMessage = CowBuffer<uint8_t>();

	if (success) {
		_draftPtr->Clear();
	}

	_draftPtr = nullptr;

	_root->Ui->Redraw();
}

CowBuffer<ObjectStorage::ID> Chat::ListThreads()
{
	CowBuffer<String> headNames =
		ListDirectory(_objectStorage.GetPath() + "/refs");

	bool hasHead = false;
	CowBuffer<ObjectStorage::ID> result(headNames.Size());

	int index = 0;

	for (uint32_t i = 0; i < headNames.Size(); i++) {
		String headName = headNames[i];

		if (headName == "Head") {
			hasHead = true;
			continue;
		}

		headName = headName.Substring(5, headName.Length() - 5);

		if (headName.Length() !=
			(int)ObjectStorage::Constants::IDSize * 2)
		{
			THROW("Invalid head name in message database.");
		}

		CowBuffer<uint8_t> value((int)ObjectStorage::Constants::IDSize);

		HexToData(headName, value.Pointer());

		result[index].SetValue(value.Pointer());
		++index;
	}

	if (hasHead) {
		result.Resize(result.Size() - 1);
	}

	return result;
}

ObjectStorage::ID Chat::GetRootMessageForThread(
	const ObjectStorage::ID &threadID)
{
	String referenceName;

	if (threadID.IsZero()) {
		referenceName = "Head";
	} else {
		referenceName = "Head_" +
			DataToHex(
				threadID.GetValuePointer(),
				(int)ObjectStorage::Constants::IDSize);
	}

	if (!_objectStorage.HasRef(referenceName)) {
		return ObjectStorage::ID();
	}

	return _objectStorage.GetRef(referenceName);
}

ObjectStorage::ID Chat::GetNextMessage(const ObjectStorage::ID &identifier)
{
	Tree<MessageTreeEntry>::Entry *entry =
		_messagesByID.FindEntry(identifier);

	if (!entry) {
		return ObjectStorage::ID();
	}

	if (!entry->Key.Node->Next) {
		return ObjectStorage::ID();
	}

	return entry->Key.Node->Next->Descriptor->Identifier;
}

ObjectStorage::ID Chat::GetPreviousMessage(const ObjectStorage::ID &identifier)
{
	Tree<MessageTreeEntry>::Entry *entry =
		_messagesByID.FindEntry(identifier);

	if (!entry) {
		return ObjectStorage::ID();
	}

	if (!entry->Key.Node->Previous) {
		return ObjectStorage::ID();
	}

	return entry->Key.Node->Previous->Descriptor->Identifier;
}

MessageDescriptor *Chat::GetMessageDescriptor(
	const ObjectStorage::ID &identifier)
{
	Tree<MessageTreeEntry>::Entry *entry =
		_messagesByID.FindEntry(identifier);

	if (!entry) {
		return nullptr;
	}

	return entry->Key.Node->Descriptor;
}

bool Chat::MessageTreeEntry::operator==(const MessageTreeEntry &e) const
{
	return Identifier == e.Identifier;
}

bool Chat::MessageTreeEntry::operator<(const MessageTreeEntry &e) const
{
	return Identifier < e.Identifier;
}

bool Chat::UnreadMessageTreeEntry::operator==(
	const UnreadMessageTreeEntry &e) const
{
	return Timestamp == e.Timestamp &&
		Index == e.Index &&
		Identifier == e.Identifier;
}

bool Chat::UnreadMessageTreeEntry::operator<(
	const UnreadMessageTreeEntry &e) const
{
	if (Timestamp != e.Timestamp) {
		return Timestamp < e.Timestamp;
	}

	if (Index != e.Index) {
		return Index < e.Index;
	}

	return Identifier < e.Identifier;
}

void Chat::LoadMessages()
{
	CowBuffer<String> headNames =
		ListDirectory(_objectStorage.GetPath() + "/refs");

	for (uint32_t headIdx = 0; headIdx < headNames.Size(); headIdx++) {
		String headName = headNames[headIdx];

		ObjectStorage::ID id = _objectStorage.GetRef(headName);
		MessageNode *newNode = nullptr;

		while (!id.IsZero() && _objectStorage.HasObject(id)) {
			CowBuffer<uint8_t> object = _objectStorage.ReadObject(
				id,
				0,
				(int)ObjectStorage::Constants::IDSize);

			MessageDescriptor *md = new MessageDescriptor(
				_root,
				&_objectStorage,
				_peerName,
				id);

			MessageNode *node = new MessageNode(md);

			node->Next = newNode;

			if (newNode) {
				newNode->Previous = node;
			}

			newNode = node;

			_messagesByID.AddEntry(node);

			bool messageIsUnreadInbound =
				md->HasAttribute(Message::Attribute::Unread) &&
				md->HasAttribute(Message::Attribute::Inbound);

			if (messageIsUnreadInbound) {
				_unreadMessages.AddEntry(node);
			}

			ObjectStorage::ID prevID(object.Pointer());
			id = prevID;

			if (!_currentMessage && headName == "Head") {
				_currentMessage = node;
			}
		}
	}
}

void Chat::UnloadMessages()
{
	Tree<MessageTreeEntry>::Entry *entry = _messagesByID.FindSmallest();

	while (entry) {
		delete entry->Key.Node;
		entry->Key.Node = nullptr;

		Tree<MessageTreeEntry>::Entry *tmp = entry;
		entry = _messagesByID.Next(entry);

		_messagesByID.RemoveEntry(tmp);
	}

	_currentMessage = nullptr;
	_currentMessageLine = 0;
}

void Chat::AddMessageToThread(
	ObjectStorage::ID messageID,
	const Message::X25519::HeaderPointToPoint &header,
	const CowBuffer<uint8_t> message,
	Message::Attribute attrs)
{
	String referenceName;

	if (header.ThreadID.IsZero()) {
		referenceName = "Head";
	} else {
		CowBuffer<uint8_t> threadIdBuffer = header.ThreadID.GetValue();
		referenceName = "Head_" +
			DataToHex(
				threadIdBuffer.Pointer(),
				threadIdBuffer.Size());
	}

	ObjectStorage::ID prevMessageID;

	if (_objectStorage.HasRef(referenceName)) {
		prevMessageID = _objectStorage.GetRef(referenceName);
	}

	CowBuffer<uint8_t> object =
		(int)ObjectStorage::Constants::IDSize +
		sizeof(Message::Attribute) +
		sizeof(uint64_t);

	int offset = 0;
	prevMessageID.GetValue(object.Pointer());
	offset += (int)ObjectStorage::Constants::IDSize;

	*object.SwitchType<Message::Attribute>(offset) = attrs;
	offset += sizeof(attrs);

	*object.SwitchType<uint64_t>(offset) = header.HeaderSize;

	CowBuffer<CowBuffer<uint8_t>> fullObject(2);
	fullObject[0] = object;
	fullObject[1] = message;

	_objectStorage.WriteObject(messageID, fullObject);
	_objectStorage.SetRef(referenceName, messageID);

	MessageDescriptor *md = new MessageDescriptor(
		_root,
		&_objectStorage,
		_peerName,
		messageID);

	MessageNode *node = new MessageNode(md);

	MessageNode *previousNode = nullptr;

	if (_messagesByID.FindEntry(prevMessageID)) {
		previousNode = _messagesByID.FindEntry(prevMessageID)->Key.Node;
	}

	node->Previous = previousNode;

	if (previousNode) {
		previousNode->Next = node;
	}

	if (previousNode == _currentMessage && !_currentMessageLine)
	{
		_currentMessage = node;
	}

	_messagesByID.AddEntry(node);

	bool messageIsUnreadInbound =
		md->HasAttribute(Message::Attribute::Unread) &&
		md->HasAttribute(Message::Attribute::Inbound);

	if (messageIsUnreadInbound) {
		_unreadMessages.AddEntry(node);
	}
}
