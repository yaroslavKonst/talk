#ifndef _MESSAGE_DESCRIPTOR_HPP
#define _MESSAGE_DESCRIPTOR_HPP

#include "Root.hpp"
#include "../Message/Message.hpp"
#include "../Common/EventDispatcher.hpp"
#include "../Crypto/Crypto.hpp"

class MessageDecryptor
{
public:
	MessageDecryptor(
		Root *root,
		String peerName,
		const Message::X25519::HeaderPointToPoint &header,
		const CowBuffer<uint8_t> message,
		Message::Contents *contents);

	bool Run();
	bool Ended();
	bool Failure();

private:
	const CowBuffer<uint8_t> _message;
	uint64_t _offset;

	bool _ended;
	bool _failure;

	CowBuffer<uint8_t> _decryptedMessage;
	uint64_t _decryptedOffset;

	CowBuffer<uint8_t> _header;

	Message::Contents *_contents;

	Crypto::X25519::EncryptedStream _stream;
	Crypto::X25519::CryptoStreamReader _streamReader;
};

class MessageEncryptor
{
public:
	MessageEncryptor(
		Root *root,
		ObjectStorage *objectStorage,
		String peerName,
		const Message::Contents *contents,
		Message::X25519::HeaderPointToPoint &header,
		CowBuffer<uint8_t> *message);

	bool Run();
	bool Ended();
	bool Failure();

private:
	CowBuffer<uint8_t> *_message;
	uint64_t _readOffset;
	uint64_t _writeOffset;
	bool _ended;
	bool _failure;

	CowBuffer<uint8_t> _body;
	CowBuffer<uint8_t> _header;

	Crypto::X25519::EncryptedStream _stream;
	Crypto::X25519::CryptoStreamWriter _streamWriter;
};

class MessageDescriptor :
	public QuantEventProcessor,
	public MessageEventProcessor::MessageDescriptorBase
{
public:
	MessageDescriptor(
		Root *root,
		ObjectStorage *objectStorage,
		String peerName,
		const ObjectStorage::ID &identifier);
	~MessageDescriptor();

	ObjectStorage::ID Identifier;

	const Message::X25519::HeaderPointToPoint &GetHeader() override;
	Message::X25519::HeaderPointToPoint Header;
	Message::Attribute Attrs;

	bool HasAttribute(Message::Attribute attr) override;
	void SetAttribute(Message::Attribute attr);
	void ClearAttribute(Message::Attribute attr);
	void SaveAttributes();

	bool HasContents() override;
	const Message::Contents &GetContents() override;
	Message::Contents Contents;

	void ProcessQuant() override;

	void RunDecryption() override;
	bool DecryptionInProgress() override;
	MessageDecryptor *Dec;
	bool DecryptionFailure() override;

	void Clear() override;

private:
	Root *_root;
	ObjectStorage *_objectStorage;

	String _peerName;

	bool _hasContents;
	bool _decryptionFailure;

	void Load();
};

#endif
