#include "Network.hpp"

#include <unistd.h>
#include <sys/socket.h>

#include "../Common/UnixTime.hpp"
#include "../Common/Exception.hpp"

Network::Network(Root *root)
{
	SetInterval(2);
	SetTimestamp(GetUnixTime());

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
	if (_handshake) {
		bool success = _handshake->ProcessRead();

		if (!success) {
			CloseConnection();
			_root->Ui->Notify("Handshake failed.");
			return;
		}

		if (_handshake->ConnectionSuccessful()) {
			_session = new ClientSession(
				_fd,
				_handshake->GetOutES(),
				_handshake->GetInES(),
				_handshake->GetOutScramblerInit(),
				_handshake->GetInScramblerInit());

			delete _handshake;
			_handshake = nullptr;
			_root->Ui->Redraw();
		}

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
	if (_handshake) {
		bool success = _handshake->ProcessWrite();

		if (!success) {
			CloseConnection();
			_root->Ui->Notify("Handshake failed.");
			return;
		}

		if (_handshake->ConnectionSuccessful()) {
			_session = new ClientSession(
				_fd,
				_handshake->GetOutES(),
				_handshake->GetInES(),
				_handshake->GetOutScramblerInit(),
				_handshake->GetInScramblerInit());

			delete _handshake;
			_handshake = nullptr;
			_root->Ui->Redraw();
		}

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
	THROW("Not implemented.");
}

bool Network::ConnectionActive()
{
	return _session;
}

bool Network::HandshakeActive()
{
	return _handshake;
}

void Network::StartConnection(int fd, const uint8_t *serverKey)
{
	_fd = fd;

	_handshake = new ClientHandshake(
		_fd,
		_root->PrivateKey,
		_root->PublicKey,
		serverKey);
}

void Network::CloseConnection()
{
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
