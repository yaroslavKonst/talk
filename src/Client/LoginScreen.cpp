#include "LoginScreen.hpp"

#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <curses.h>
#include <cerrno>

#include "WorkScreen.hpp"
#include "TextColor.hpp"
#include "../Common/Hex.hpp"
#include "../Common/File.hpp"
#include "../Common/Resolver.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

LoginScreen::LoginScreen(Root *root)
{
	_writingIp = true;
	_writingPort = false;
	_writingKey = false;

	_root = root;

	_ip.Caption = "Host name: ";
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

	UiHelpers::DrawFrame(
		5,
		_rows - 3,
		1,
		_columns - 2,
		"Login to server as " + _root->Conf->GetName(),
		COLOR_PAIR(YELLOW_TEXT));

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
		(event >= 'a' && event <= 'z') ||
		(event >= 'A' && event <= 'Z') ||
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
		return nullptr;
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

CowBuffer<String> LoginScreen::GetControlHelp()
{
	CowBuffer<String> result(3);
	result[0] = "Back: " + _root->Conf->LoginBackName();
	result[1] = "Up/Down: " + _root->Conf->LoginUpName() + "/" +
		_root->Conf->LoginDownName();
	result[2] = "Connect: " + _root->Conf->LoginConnectName();

	return result;
}

Screen *LoginScreen::ProcessConnection()
{
	if (_root->Network->ConnectionActive() ||
		_root->Network->HandshakeActive())
	{
		_root->Ui->Notify("Connection is already established.");
		return this;
	}

	if (_serverKeyHex.Text.Length() != Crypto::X25519::KEY_SIZE * 2) {
		_root->Ui->Notify("Invalid server key length.");
		return this;
	}

	Crypto::X25519::PublicKeyContainer serverKey;
	HexToData(_serverKeyHex.Text, serverKey.Key);

	void *blockHandle = _root->Ui->BlockNotify("Connecting...");

	Resolver resolver(_root->Dispatcher);
	resolver.Resolve(_ip.Text, _port.Text, SOCK_STREAM);
	int res = resolver.GetResolveStatus();

	if (res) {
		_root->Ui->BlockCancel(blockHandle);
		_root->Ui->Notify("Failed to resolve host name.");
		return this;
	}

	struct addrinfo *addrs = resolver.GetResolveResult();

	if (!addrs) {
		_root->Ui->BlockCancel(blockHandle);
		_root->Ui->Notify("Host name has no addresses.");
		return this;
	}

	if (_modified) {
		_root->Conf->SetServerAddress(_ip.Text);
		_root->Conf->SetServerPort(_port.Text);
		_root->Conf->SetServerKeyHex(_serverKeyHex.Text);
		_root->Conf->Save();
		_modified = false;
	}

	int socketFd = -1;

	while (addrs) {
		socketFd = socket(addrs->ai_family, addrs->ai_socktype, 0);

		if (socketFd == -1) {
			addrs = addrs->ai_next;
			continue;
		}

		res = connect(
			socketFd,
			addrs->ai_addr,
			addrs->ai_addrlen);

		if (res != -1) {
			break;
		}

		close(socketFd);
		addrs = addrs->ai_next;
	}

	resolver.Clear();

	if (socketFd == -1) {
		_root->Ui->BlockCancel(blockHandle);
		_root->Ui->Notify(
			String("Failed to create socket: ") +
			strerror(errno) + ".");
		return this;
	}

	if (res == -1) {
		_root->Ui->BlockCancel(blockHandle);
		_root->Ui->Notify(
			String("Failed to connect: ") + strerror(errno) + ".");
		return this;
	}

	MakeNonblocking(socketFd);

	_root->Ui->BlockCancel(blockHandle);

	_root->Network->StartConnection(socketFd, serverKey);

	return nullptr;
}
