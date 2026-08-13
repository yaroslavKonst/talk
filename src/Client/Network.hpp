#ifndef _NETWORK_HPP
#define _NETWORK_HPP

#include "Root.hpp"
#include "ClientHandshake.hpp"
#include "ClientSession.hpp"

class Network :
	public NetworkEventProcessor,
	public DescriptorEventProcessor,
	public TimeEventProcessor
{
public:
	Network(Root *root);
	~Network();

	int GetDescriptor() override
	{
		return _fd;
	}

	bool RequestRead() override;
	bool RequestWrite() override;
	void ProcessRead() override;
	void ProcessWrite() override;

	void ProcessTimeEvent() override;

	bool ConnectionActive() override;
	bool HandshakeActive() override;

	void StartConnection(
		int fd,
		const Crypto::X25519::PublicKeyContainer &serverKey) override;
	void Disconnect() override;

	uint64_t GetMaxMessageSize() override;

	bool AddContact(String name) override;
	bool UpdateContactKey(
		String contactName,
		const Crypto::X25519::PublicKeyContainer &key,
		bool validated,
		bool blocked,
		bool setAsDefault) override;
	bool BlockContact(
		String contactName,
		Contact::BlockStatus block) override;
	bool RemoveContact(String name) override;

	bool ListContacts();
	bool SetContactListProcessor(ContactListProcessor *processor);

	bool SendMessage(const CowBuffer<uint8_t> message);
	bool UpdateMessage(
		String peerName,
		const ObjectStorage::ID &messageID,
		Message::Attribute attr,
		bool value) override;

	bool RequestAccountSettings() override;
	bool SetAccountSettings(
		bool allowMessages,
		bool allowCalls) override;
	bool SetAccountSettingsProcessor(
		AccountSettingsProcessor *processor) override;

private:
	int _fd;
	Root *_root;

	ClientHandshake *_handshake;
	ClientSession *_session;

	void CheckHandshake();
	void CloseConnection();
};

#endif
