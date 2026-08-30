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
#include "UiLayout.hpp"
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

	_ip.SetText(_root->Conf->GetServerAddress());
	_port.SetText(_root->Conf->GetServerPort());
	_serverKeyHex.SetText(_root->Conf->GetServerKeyHex());

	_modified = false;

	if (_ip.HasText()) {
		_writingIp = false;
		_writingPort = true;
	}

	if (_writingPort && _port.HasText()) {
		_writingPort = false;
		_writingKey = true;
	}
}

void LoginScreen::Redraw()
{
	for (int i = 0; i < _columns; i++) {
		move(LayoutConstants::HeaderHeight, i);
		addch(ACS_HLINE);
	}

	bool running;
	UiHelpers::DrawFrame(
		LayoutConstants::HeaderHeight + 1,
		_rows - 1 - LayoutConstants::FooterHeight,
		1,
		_columns - 2,
		"Login to server as " + _root->Conf->GetName(),
		COLOR_PAIR(YELLOW_TEXT),
		running);

	if (running) {
		_root->Ui->RequestRunningLine();
	}

	_ip.SetCaptionPosition(_rows / 2 - 2, 3);
	_ip.SetWidthLimit(_columns - 6);
	_ip.AlignTextToCaption();
	_ip.Redraw();

	_port.SetCaptionPosition(_rows / 2, 3);
	_port.SetWidthLimit(_columns - 6);
	_port.AlignTextToCaption();
	_port.Redraw();

	_serverKeyHex.SetCaptionPosition(_rows / 2 + 2, 3);
	_serverKeyHex.SetWidthLimit(_columns - 6);
	_serverKeyHex.SetTextPosition(_rows / 2 + 3, 3);
	_serverKeyHex.Redraw();

	if (_writingIp) {
		_ip.SetCursor();
	} else if (_writingPort) {
		_port.SetCursor();
	} else if (_writingKey) {
		_serverKeyHex.SetCursor();
	}
}

static bool ValidIpChar(int event)
{
	return
		(event > ' ' && event <= '~') ||
		event == '\b';
}

static bool ValidPortChar(int event)
{
	return
		(event >= '0' && event <= '9') ||
		event == '\b';
}

static bool ValidKeyChar(int event)
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
		if (!ValidIpChar(event)) {
			_root->Ui->Notify("Illegal character.");
			return this;
		}

		_ip.ProcessChar(event);
		_modified = true;
	} else if (_writingPort) {
		if (!ValidPortChar(event)) {
			_root->Ui->Notify("Illegal character.");
			return this;
		}

		_port.ProcessChar(event);
		_modified = true;
	} else if (_writingKey) {
		if (!ValidKeyChar(event)) {
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

	String serverKeyHex = _serverKeyHex.GetText();

	if (serverKeyHex.Length() != Crypto::X25519::KEY_SIZE * 2) {
		_root->Ui->Notify("Invalid server key length.");
		return this;
	}

	Crypto::X25519::PublicKeyContainer serverKey;
	HexToData(serverKeyHex, serverKey.Key);

	void *blockHandle = _root->Ui->BlockNotify("Connecting...");

	Resolver resolver(_root->Dispatcher);

	String hostName = _ip.GetText();
	String portName = _port.GetText();

	IPAddress address;
	uint16_t portNumber;

	if (!portName.Length()) {
		Resolver::SRVResult *srv = resolver.ResolveSRV(
			hostName,
			"talkdclient",
			true);

		if (resolver.GetResolveStatus() || !srv) {
			if (srv) {
				delete srv;
			}

			_root->Ui->BlockCancel(blockHandle);
			_root->Ui->Notify("Failed to resolve host name/port.");
			return this;
		}

		hostName = srv->Target;
		portNumber = srv->Port;

		delete srv;
	} else {
		if (!Message::VerifyPortName(portName)) {
			_root->Ui->BlockCancel(blockHandle);
			_root->Ui->Notify("Invalid port number.");
			return this;
		}

		int port = atoi(portName.CStr());

		portNumber = htons(port);
	}

	if (!address.ParseIPAddress(hostName)) {
		address = resolver.ResolveA(hostName);

		if (resolver.GetResolveStatus()) {
			address = resolver.ResolveAAAA(hostName);

			if (resolver.GetResolveStatus()) {
				_root->Ui->BlockCancel(blockHandle);
				_root->Ui->Notify(
					"Failed to resolve host address.");
				return this;
			}
		}
	}

	if (_modified) {
		_root->Conf->SetServerAddress(_ip.GetText());
		_root->Conf->SetServerPort(_port.GetText());
		_root->Conf->SetServerKeyHex(_serverKeyHex.GetText());
		_root->Conf->Save();
		_modified = false;
	}

	int addrLen;
	struct sockaddr_storage *addr =
		address.GetStructSockaddr(portNumber, addrLen);

	int socketFd = socket(addr->ss_family, SOCK_STREAM, 0);

	if (socketFd == -1) {
		delete addr;
		_root->Ui->BlockCancel(blockHandle);
		_root->Ui->Notify("Failed to create socket.");
		return this;
	}

	int res = connect(socketFd, (struct sockaddr*)addr, addrLen);

	delete addr;

	if (res == -1) {
		close(socketFd);
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
