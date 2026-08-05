#include "MessageDescriptor.hpp"

#include "../Message/Message.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/Hex.hpp"
#include "../Common/File.hpp"
#include "../Common/Endianness.hpp"

static const int MessageBlockSize = 2048;
static const int EncryptedMessageBlockSize =
	MessageBlockSize + Crypto::X25519::MAC_SIZE;

static bool GenerateSymmetricKey(
	Root *root,
	String peerName,
	const Message::X25519::HeaderPointToPoint &header,
	Crypto::X25519::SymmetricKeyContainer &symmetricKey)
{
	Crypto::X25519::SymmetricKeyContainer unusedKey;

	bool outboundMessage = header.Destination == peerName;

	CowBuffer<uint8_t> salt(
		sizeof(header.Timestamp) + sizeof(header.Index));
	*salt.SwitchType<int64_t>() = SetProtoEndian(header.Timestamp);
	*salt.SwitchType<int32_t>(sizeof(int64_t)) =
		SetProtoEndian(header.Index);

	bool success;

	if (outboundMessage) {
		success = Crypto::X25519::GenerateSessionKeys(
			*root->PrivateKey,
			*root->PublicKey,
			header.DestinationKey,
			salt,
			symmetricKey,
			unusedKey,
			false);
	} else {
		success = Crypto::X25519::GenerateSessionKeys(
			*root->PrivateKey,
			*root->PublicKey,
			header.SourceKey,
			salt,
			symmetricKey,
			unusedKey,
			true);
	}

	return success;
}

MessageDecryptor::MessageDecryptor(
	Root *root,
	String peerName,
	const Message::X25519::HeaderPointToPoint &header,
	const CowBuffer<uint8_t> message,
	Message::Contents *contents) :
	_message(message)
{
	_contents = contents;

	_ended = false;
	_failure = false;

	if (header.HeaderSize + header.MessageSize != _message.Size()) {
		_failure = true;
		return;
	}

	_offset = header.HeaderSize;
	_decryptedOffset = 0;
	_header = _message.Slice(0, header.HeaderSize);

	int blockCount = (header.MessageSize + EncryptedMessageBlockSize - 1) /
		EncryptedMessageBlockSize;

	_decryptedMessage = CowBuffer<uint8_t>(
		header.MessageSize - blockCount * Crypto::X25519::MAC_SIZE);

	bool keyIsValid = GenerateSymmetricKey(
		root,
		peerName,
		header,
		_stream.Key);

	if (!keyIsValid) {
		_failure = true;
		return;
	}

	memset(_stream.Nonce, 0, Crypto::X25519::NONCE_SIZE);

	if (!_streamReader.Init(&_stream, header.Nonce)) {
		_failure = true;
		return;
	}
}

bool MessageDecryptor::Run()
{
	if (_failure) {
		return false;
	}

	if (_ended) {
		return true;
	}

	int nextBlockSize = _message.Size() - _offset;

	if (nextBlockSize > EncryptedMessageBlockSize) {
		nextBlockSize = EncryptedMessageBlockSize;
	}

	CowBuffer<uint8_t> decryptedBlock = _streamReader.Decrypt(
		_message.Slice(_offset, nextBlockSize),
		_header);

	if (!decryptedBlock.Size()) {
		_failure = true;
		return false;
	}

	_offset += nextBlockSize;

	memcpy(
		_decryptedMessage.Pointer(_decryptedOffset),
		decryptedBlock.Pointer(),
		decryptedBlock.Size());

	_decryptedOffset += decryptedBlock.Size();

	if (_offset >= _message.Size()) {
		if (_decryptedOffset != _decryptedMessage.Size()) {
			_failure = true;
			return false;
		}

		bool parsingSuccess =
			Message::ParseContents(_decryptedMessage, *_contents);

		if (!parsingSuccess) {
			_failure = true;
			return false;
		}

		_ended = true;
		return true;
	}

	return true;
}

bool MessageDecryptor::Ended()
{
	return _ended;
}

bool MessageDecryptor::Failure()
{
	return _failure;
}

MessageEncryptor::MessageEncryptor(
	Root *root,
	ObjectStorage *objectStorage,
	String peerName,
	const Message::Contents *contents,
	Message::X25519::HeaderPointToPoint &header,
	CowBuffer<uint8_t> *message)
{
	_message = message;

	_readOffset = 0;
	_writeOffset = 0;
	_ended = false;
	_failure = false;

	uint8_t emptyEntrySize;
	Crypto::GenerateRandomData(1, &emptyEntrySize, false);

	_body = Message::BuildContents(*contents, emptyEntrySize);

	int blockCount = (_body.Size() + MessageBlockSize - 1) /
		MessageBlockSize;

	header.MessageSize = _body.Size() +
		blockCount * Crypto::X25519::MAC_SIZE;

	Message::X25519::WriteHeaderSize(header);

	bool messageIsTooBig =
		sizeof(int32_t) * 2 + header.HeaderSize + header.MessageSize >
		root->Network->GetMaxMessageSize();

	if (messageIsTooBig) {
		_failure = true;
		root->Ui->Notify(
			"Message is too big. Limit is " +
			DataSizeToString(
				root->Network->GetMaxMessageSize() -
				sizeof(int32_t) * 2) +
			".");
		return;
	}

	Crypto::X25519::InitNonce(_stream.Nonce);
	memcpy(header.Nonce, _stream.Nonce, Crypto::X25519::NONCE_SIZE);

	header.Index = 0;

	for (;;) {
		_header = Message::X25519::BuildHeader(header);

		CowBuffer<uint8_t> hashBuffer = Crypto::GetHash(
			_header,
			(int)ObjectStorage::Constants::IDSize);
		ObjectStorage::ID id;
		id.SetValue(hashBuffer.Pointer());

		if (!id.IsZero() && !objectStorage->HasObject(id)) {
			break;
		}

		header.Index += 1;

		if (header.Index < 0) {
			_failure = true;
			root->Ui->Notify("Failed to get free message index.");
			return;
		}
	}

	bool keyIsValid = GenerateSymmetricKey(
		root,
		peerName,
		header,
		_stream.Key);

	if (!keyIsValid) {
		_failure = true;
		root->Ui->Notify("Key exchange for message failed.");
		return;
	}

	_streamWriter.Init(&_stream);

	_header = Message::X25519::BuildHeader(header);

	*_message = CowBuffer<uint8_t>(_header.Size() + header.MessageSize);
	memcpy(_message->Pointer(), _header.Pointer(), _header.Size());
	_writeOffset = _header.Size();
}

bool MessageEncryptor::Run()
{
	if (_failure) {
		return false;
	}

	if (_ended) {
		return true;
	}

	uint64_t nextBlockSize = _body.Size() - _readOffset;

	if (nextBlockSize > MessageBlockSize) {
		nextBlockSize = MessageBlockSize;
	}

	CowBuffer<uint8_t> encryptedBlock = _streamWriter.Encrypt(
		_body.Slice(_readOffset, nextBlockSize),
		_header);

	memcpy(
		_message->Pointer(_writeOffset),
		encryptedBlock.Pointer(),
		encryptedBlock.Size());

	_readOffset += nextBlockSize;
	_writeOffset += EncryptedMessageBlockSize;

	if (_readOffset >= _body.Size()) {
		_ended = true;
	}

	return true;
}

bool MessageEncryptor::Ended()
{
	return _ended;
}

bool MessageEncryptor::Failure()
{
	return _failure;
}

MessageDescriptor::MessageDescriptor(
	Root *root,
	ObjectStorage *objectStorage,
	String peerName,
	const ObjectStorage::ID &identifier)
{
	_root = root;
	_objectStorage = objectStorage;
	_peerName = peerName;

	Identifier = identifier;

	_hasContents = false;
	_decryptionFailure = false;

	Attrs = (Message::Attribute)0;

	Dec = nullptr;

	Load();
}

MessageDescriptor::~MessageDescriptor()
{
	if (Dec) {
		delete Dec;
		Dec = nullptr;
	}

	_root->Dispatcher->UnregisterQuantProcessor(this);
}

const Message::X25519::HeaderPointToPoint &MessageDescriptor::GetHeader()
{
	return Header;
}

bool MessageDescriptor::HasAttribute(Message::Attribute attr)
{
	return Message::AttributeAction::Has(Attrs, attr);
}

void MessageDescriptor::SetAttribute(Message::Attribute attr)
{
	Attrs = Message::AttributeAction::Set(Attrs, attr);
	SaveAttributes();
}

void MessageDescriptor::ClearAttribute(Message::Attribute attr)
{
	Attrs = Message::AttributeAction::Clear(Attrs, attr);
	SaveAttributes();
}

void MessageDescriptor::SaveAttributes()
{
	CowBuffer<uint8_t> attrBuffer(sizeof(Message::Attribute));
	*attrBuffer.SwitchType<Message::Attribute>() = Attrs;

	_objectStorage->UpdateObject(
		Identifier,
		attrBuffer,
		(int)ObjectStorage::Constants::IDSize);
}

bool MessageDescriptor::HasContents()
{
	return _hasContents;
}

const Message::Contents &MessageDescriptor::GetContents()
{
	return Contents;
}

void MessageDescriptor::ProcessQuant()
{
	if (!Dec) {
		return;
	}

	bool success = Dec->Run();

	if (!success || Dec->Failure()) {
		delete Dec;
		Dec = nullptr;
		_decryptionFailure = true;
		_root->Ui->Redraw();
		return;
	}

	if (Dec->Ended()) {
		delete Dec;
		Dec = nullptr;
		_hasContents = true;
		_root->Ui->Redraw();
		return;
	}

	_root->Dispatcher->RegisterQuantProcessor(this);
}

void MessageDescriptor::RunDecryption()
{
	if (Dec || _decryptionFailure || _hasContents) {
		return;
	}

	CowBuffer<uint8_t> messageBuffer =
		_objectStorage->ReadObject(Identifier);

	uint64_t prefixSize = (int)ObjectStorage::Constants::IDSize +
		sizeof(Attrs) + sizeof(uint64_t);

	Dec = new MessageDecryptor(
		_root,
		_peerName,
		Header,
		messageBuffer.Slice(
			prefixSize,
			messageBuffer.Size() - prefixSize),
		&Contents);

	if (Dec->Failure()) {
		delete Dec;
		Dec = nullptr;
		_decryptionFailure = true;
		_root->Ui->Redraw();
		return;
	}

	_root->Dispatcher->RegisterQuantProcessor(this);
}

bool MessageDescriptor::DecryptionInProgress()
{
	return Dec;
}

bool MessageDescriptor::DecryptionFailure()
{
	return _decryptionFailure;
}

void MessageDescriptor::Clear()
{
	_root->Dispatcher->UnregisterQuantProcessor(this);

	if (Dec) {
		delete Dec;
		Dec = nullptr;
	}

	Contents.Clear();

	_hasContents = false;
	_decryptionFailure = false;
}

void MessageDescriptor::Load()
{
	CowBuffer<uint8_t> objectHeader = _objectStorage->ReadObject(
		Identifier,
		(int)ObjectStorage::Constants::IDSize,
		sizeof(Attrs) + sizeof(uint64_t));

	Attrs = *objectHeader.SwitchType<Message::Attribute>();
	uint64_t headerSize = *objectHeader.SwitchType<uint64_t>(sizeof(Attrs));

	CowBuffer<uint8_t> headerBuffer = _objectStorage->ReadObject(
		Identifier,
		(int)ObjectStorage::Constants::IDSize + objectHeader.Size(),
		headerSize);

	bool parseResult = Message::X25519::ParseHeader(headerBuffer, Header);

	if (!parseResult) {
		THROW("Local database corruption, failed to parse "
			"message header.");
	}
}
