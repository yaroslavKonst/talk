#include "Network.hpp"

#include <unistd.h>
#include <sys/socket.h>

#include "../Common/UnixTime.hpp"
#include "../Common/Exception.hpp"

Network::Network(Root *root)
{
	SetInterval(2000);
	SetTimestamp(GetMonotonicMillisecondTime());

	_fd = -1;
	_root = root;

	_handshake = nullptr;
	_session = nullptr;
}

Network::~Network()
{
	CloseConnection();
}

bool Network::RequestRead()
{
	if (_handshake) {
		return _handshake->RequestRead();
	}

	if (_session) {
		return _session->RequestRead();
	}

	return false;
}

bool Network::RequestWrite()
{
	if (_handshake) {
		return _handshake->RequestWrite();
	}

	if (_session) {
		return _session->RequestWrite();
	}

	return false;
}

void Network::ProcessRead()
{
	SetTimestamp(GetMonotonicMillisecondTime());

	if (_handshake) {
		bool success = _handshake->ProcessRead();

		if (!success) {
			CloseConnection();
			_root->Ui->Notify("Handshake failed.");
			return;
		}

		CheckHandshake();
		return;
	}

	if (_session) {
		bool success = _session->ProcessRead();

		if (!success) {
			CloseConnection();
			_root->Ui->Notify("Connection lost.");
			return;
		}

		return;
	}

	THROW("Session is inactive.");
}

void Network::ProcessWrite()
{
	SetTimestamp(GetMonotonicMillisecondTime());

	if (_handshake) {
		bool success = _handshake->ProcessWrite();

		if (!success) {
			CloseConnection();
			_root->Ui->Notify("Handshake failed.");
			return;
		}

		CheckHandshake();
		return;
	}

	if (_session) {
		bool success = _session->ProcessWrite();

		if (!success) {
			CloseConnection();
			_root->Ui->Notify("Connection lost.");
			return;
		}

		return;
	}

	THROW("Session is inactive.");
}

void Network::ProcessTimeEvent()
{
	bool kaSuccess = false;

	if (_session) {
		kaSuccess = _session->InitKeepAlive();
	}

	if (kaSuccess) {
		return;
	}

	CloseConnection();
	_root->Ui->Notify("Connection lost due to timeout.");
}

bool Network::ConnectionActive()
{
	return _session;
}

bool Network::HandshakeActive()
{
	return _handshake;
}

void Network::StartConnection(
	int fd,
	const Crypto::X25519::PublicKeyContainer &serverKey)
{
	SetTimestamp(GetMonotonicMillisecondTime());

	_fd = fd;

	String name = _root->Conf->GetName();

	if (name.Length() == 0) {
		CloseConnection();
		_root->Ui->Notify("Please specify user name.");
		return;
	}

	_handshake = new ClientHandshake(
		_root,
		_fd,
		name,
		*_root->PrivateKey,
		*_root->PublicKey,
		serverKey);

	if (_handshake->ErrorState()) {
		CloseConnection();
		return;
	}

	_root->Dispatcher->RegisterDescriptorProcessor(this);
	_root->Dispatcher->RegisterTimeProcessor(this);
}

void Network::Disconnect()
{
	CloseConnection();
	_root->Ui->Redraw();
}

uint64_t Network::GetMaxMessageSize()
{
	if (_session) {
		return _session->GetMaxMessageSize();
	}

	return 2048;
}

bool Network::AddContact(String name)
{
	if (!_session) {
		return false;
	}

	_session->AddContact(name);
	return true;
}

bool Network::UpdateContactKey(
	String contactName,
	const Crypto::X25519::PublicKeyContainer &key,
	bool validated,
	bool blocked,
	bool setAsDefault)
{
	if (!_session) {
		return false;
	}

	_session->UpdateContactKey(
		contactName,
		key,
		validated,
		blocked,
		setAsDefault);
	return true;
}

bool Network::BlockContact(String contactName, Contact::BlockStatus block)
{
	if (!_session) {
		return false;
	}

	_session->BlockContact(contactName, block);
	return true;
}

bool Network::RemoveContact(String name)
{
	if (!_session) {
		return false;
	}

	_session->RemoveContact(name);
	return true;
}

bool Network::ListContacts()
{
	if (!_session) {
		return false;
	}

	_session->ListContacts();
	return true;
}

bool Network::SetContactListProcessor(ContactListProcessor *processor)
{
	if (!_session) {
		return false;
	}

	_session->SetContactListProcessor(processor);
	return true;
}

bool Network::SendMessage(const CowBuffer<uint8_t> message)
{
	if (!_session) {
		return false;
	}

	_session->SendMessage(message);
	return true;
}

bool Network::UpdateMessage(
	String peerName,
	const ObjectStorage::ID &messageID,
	Message::Attribute attr,
	bool value)
{
	if (!_session) {
		return false;
	}

	_session->UpdateMessage(peerName, messageID, attr, value);
	return true;
}

bool Network::RequestAccountSettings()
{
	if (!_session) {
		return false;
	}

	_session->GetAccountSettings();
	return true;
}

bool Network::SetAccountSettings(bool allowMessages, bool allowCalls)
{
	if (!_session) {
		return false;
	}

	_session->SetAccountSettings(allowMessages, allowCalls);
	return true;
}

bool Network::SetAccountSettingsProcessor(AccountSettingsProcessor *processor)
{
	if (!_session) {
		return false;
	}

	_session->SetAccountSettingsProcessor(processor);
	return true;
}

void Network::CheckHandshake()
{
	if (_handshake->ConnectionSuccessful()) {
		_session = new ClientSession(
			_root,
			_fd,
			_handshake->GetOutES(),
			_handshake->GetInES(),
			_handshake->GetOutScramblerInit(),
			_handshake->GetInScramblerInit());

		delete _handshake;
		_handshake = nullptr;
		_root->Ui->Redraw();
	}
}

void Network::CloseConnection()
{
	_root->Dispatcher->UnregisterDescriptorProcessor(this);
	_root->Dispatcher->UnregisterTimeProcessor(this);

	if (_handshake) {
		delete _handshake;
		_handshake = nullptr;
	}

	if (_session) {
		delete _session;
		_session = nullptr;
	}

	if (_fd != -1) {
		shutdown(_fd, SHUT_RDWR);
		close(_fd);
		_fd = -1;
	}
}
