#include "LoginScreen.hpp"

#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <curses.h>

#include "WorkScreen.hpp"
#include "../Common/Hex.hpp"
#include "../Common/File.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

LoginScreen::LoginScreen(Root *root)
{
	_writingIp = true;
	_writingPort = false;
	_writingKey = false;

	_root = root;

	_ip.Caption = "IP address: ";
	_port.Caption = "Port: ";
	_serverKeyHex.Caption = "Server public key: ";

	_ip.Text = _root->Conf->GetServerAddress();
	_port.Text = _root->Conf->GetServerPort();
	_serverKeyHex.Text = _root->Conf->GetServerKeyHex();

	_modified = false;

	if (_ip.Text.Length()) {
		_writingIp = false;
		_writingPort = true;
	}

	if (_writingPort && _port.Text.Length()) {
		_writingPort = false;
		_writingKey = true;
	}
}

void LoginScreen::Redraw()
{
	for (int i = 0; i < _columns; i++) {
		move(4, i);
		addch(ACS_HLINE);
	}

	String helpString =
		"Exit: " + _root->Conf->LoginBackName() +
		" | Next: " + _root->Conf->LoginDownName() + "/" +
		_root->Conf->LoginConnectName() +
		" | Previous: " + _root->Conf->LoginUpName() +
		" | Connect: " + _root->Conf->LoginConnectName();

	move(5, 0);
	addstr(helpString.CStr());

	_ip.SetCaptionPosition(_rows / 2 - 2, 4);
	_ip.AlignTextToCaption();
	_ip.Redraw();

	_port.SetCaptionPosition(_rows / 2, 4);
	_port.AlignTextToCaption();
	_port.Redraw();

	_serverKeyHex.SetCaptionPosition(_rows / 2 + 2, 4);
	_serverKeyHex.SetTextPosition(_rows / 2 + 3, 4);
	_serverKeyHex.Redraw();

	if (_writingIp) {
		_ip.SetCursor();
	} else if (_writingPort) {
		_port.SetCursor();
	} else if (_writingKey) {
		_serverKeyHex.SetCursor();
	}
}

static bool LegalIpChar(int event)
{
	return
		(event >= '0' && event <= '9') ||
		event == '.' ||
		event == '\b';
}

static bool LegalPortChar(int event)
{
	return
		(event >= '0' && event <= '9') ||
		event == '\b';
}

static bool LegalKeyChar(int event)
{
	return
		(event >= '0' && event <= '9') ||
		(event >= 'a' && event <= 'f') ||
		event == '\b';
}

Screen *LoginScreen::ProcessEvent(int event)
{
	if (event == _root->Conf->LoginBackKey()) {
		return new WorkScreen(_root);
	}

	if (event == _root->Conf->LoginUpKey()) {
		if (_writingPort) {
			_writingPort = false;
			_writingIp = true;
		} else if (_writingKey) {
			_writingKey = false;
			_writingPort = true;
		}

		return this;
	}

	if (event == _root->Conf->LoginDownKey()) {
		if (_writingIp) {
			_writingIp = false;
			_writingPort = true;
		} else if (_writingPort) {
			_writingPort = false;
			_writingKey = true;
		}

		return this;
	}

	if (event == _root->Conf->LoginConnectKey()) {
		if (_writingIp) {
			_writingIp = false;
			_writingPort = true;
			return this;
		} else if (_writingPort) {
			_writingPort = false;
			_writingKey = true;
			return this;
		}

		return ProcessConnection();
	}

	if (_writingIp) {
		if (!LegalIpChar(event)) {
			_root->Ui->Notify("Illegal character.");
			return this;
		}

		_ip.ProcessChar(event);
		_modified = true;
	} else if (_writingPort) {
		if (!LegalPortChar(event)) {
			_root->Ui->Notify("Illegal character.");
			return this;
		}

		_port.ProcessChar(event);
		_modified = true;
	} else if (_writingKey) {
		if (!LegalKeyChar(event)) {
			_root->Ui->Notify("Illegal character.");
			return this;
		}

		_serverKeyHex.ProcessChar(event);
		_modified = true;
	}

	return this;
}

Screen *LoginScreen::ProcessConnection()
{
	if (_root->Network->ConnectionActive() ||
		_root->Network->HandshakeActive())
	{
		_root->Ui->Notify("Connection is already established.");
		return this;
	}

	if (_serverKeyHex.Text.Length() != KEY_SIZE * 2) {
		_root->Ui->Notify("Invalid server key length.");
		return this;
	}

	uint8_t serverKey[KEY_SIZE];
	HexToData(_serverKeyHex.Text, serverKey);

	uint16_t port = atoi(_port.Text.CStr());

	if (port == 0) {
		_root->Ui->Notify("Invalid port number.");
		return this;
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	int res = inet_aton(_ip.Text.CStr(), &addr.sin_addr);

	if (!res) {
		_root->Ui->Notify("Invalid IP address.");
		return this;
	}

	if (_modified) {
		_root->Conf->SetServerAddress(_ip.Text);
		_root->Conf->SetServerPort(_port.Text);
		_root->Conf->SetServerKeyHex(_serverKeyHex.Text);
		_root->Conf->Save();
		_modified = false;
	}

	void *blockHandle = _root->Ui->BlockNotify("Connecting...");

	int socketFd = socket(AF_INET, SOCK_STREAM, 0);

	if (socketFd == -1) {
		_root->Ui->BlockCancel(blockHandle);
		_root->Ui->Notify("Failed to create socket.");
		return this;
	}

	res = connect(
		socketFd,
		(struct sockaddr*)&addr,
		sizeof(addr));

	if (res == -1) {
		_root->Ui->BlockCancel(blockHandle);
		close(socketFd);
		_root->Ui->Notify("Failed to connect.");
		return this;
	}

	MakeNonblocking(socketFd);

	_root->Ui->BlockCancel(blockHandle);

	_root->Network->StartConnection(socketFd, serverKey);

	return new WorkScreen(_root);
}
