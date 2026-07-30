#ifndef _CLIENT_SESSION_HPP
#define _CLIENT_SESSION_HPP

#include "Root.hpp"
#include "../Protocol/SessionProtocol.hpp"
#include "../Crypto/Crypto.hpp"

class ClientSession
{
public:
	ClientSession(
		Root *root,
		int _fd,
		Crypto::X25519::EncryptedStream &outES,
		Crypto::X25519::EncryptedStream &inES,
		uint8_t outScramblerInit,
		uint8_t inScramblerInit);
	~ClientSession();

	bool RequestRead();
	bool RequestWrite();
	bool ProcessRead();
	bool ProcessWrite();

	bool InitKeepAlive();
	void AddContact(String name);
	void UpdateContactKey(
		String contactName,
		const Crypto::X25519::PublicKeyContainer &key,
		bool validated,
		bool blocked,
		bool setAsDefault);
	void BlockContact(String contactName, Contact::BlockStatus block);

	void ListContacts();
	void SetContactListProcessor(
		NetworkEventProcessor::ContactListProcessor *processor);

	void SendMessage(const CowBuffer<uint8_t> message);

private:
	Root *_root;

	int _fd;
	Crypto::X25519::EncryptedStream _outES;
	Crypto::X25519::EncryptedStream _inES;

	SessionProtocol *_protocol;

	int64_t _keepAliveTimestamp;

	bool ProcessInput(const CowBuffer<uint8_t> buffer);

	bool ProcessKeepAlive(const CowBuffer<uint8_t> buffer);
	void SendKeepAlive();

	void RequestHostName();
	bool ProcessGetHostName(const CowBuffer<uint8_t> buffer);
	bool ProcessRequestID();
	bool ProcessUpdateID(const CowBuffer<uint8_t> buffer);
	bool ProcessAddContact(const CowBuffer<uint8_t> buffer);
	bool ProcessUpdateContactKey(const CowBuffer<uint8_t> buffer);
	bool ProcessBlockContact(const CowBuffer<uint8_t> buffer);
	bool ProcessListContacts(const CowBuffer<uint8_t> buffer);
	NetworkEventProcessor::ContactListProcessor *_contactListProcessor;

	bool ProcessOfferMessage(const CowBuffer<uint8_t> buffer);
	bool ProcessSendMessage(const CowBuffer<uint8_t> buffer);
	bool ProcessUpdateMessage(const CowBuffer<uint8_t> buffer);
};

#endif
