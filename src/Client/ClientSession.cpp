#include "ClientSession.hpp"

#include "../Protocol/SessionParser.hpp"
#include "../Common/Exception.hpp"
#include "../Common/UnixTime.hpp"

ClientSession::ClientSession(
	int fd,
	EncryptedStream &outES,
	EncryptedStream &inES,
	uint8_t outScramblerInit,
	uint8_t inScramblerInit)
{
	_fd = fd;
	_inES = inES;
	_outES = outES;

	_keepAliveTimestamp = 0;

	_protocol = new SessionProtocol(
		fd,
		&_outES,
		&_inES,
		outScramblerInit,
		inScramblerInit);
}

ClientSession::~ClientSession()
{
	delete _protocol;
}

bool ClientSession::RequestRead()
{
	return true;
}

bool ClientSession::RequestWrite()
{
	return _protocol->RequestWrite();
}

bool ClientSession::ProcessRead()
{
	bool success = _protocol->Read();

	if (!success) {
		return false;
	}

	if (!_protocol->CanReceive()) {
		return true;
	}

	return ProcessInput(_protocol->Receive());
}

bool ClientSession::ProcessWrite()
{
	return _protocol->Write();
}

bool ClientSession::InitKeepAlive()
{
	if (_keepAliveTimestamp) {
		return false;
	}

	SendKeepAlive();
	return true;
}

bool ClientSession::ProcessInput(const CowBuffer<uint8_t> buffer)
{
	if (buffer.Size() < sizeof(int32_t)) {
		return false;
	}

	int32_t command = *buffer.SwitchType<int32_t>();

	switch (command) {
	case SESSION_COMMAND_KEEP_ALIVE:
		return ProcessKeepAlive(buffer);
	default:
		return false;
	}
}

bool ClientSession::ProcessKeepAlive(const CowBuffer<uint8_t> buffer)
{
	CommandKeepAlive::Command request;
	bool parseResult = CommandKeepAlive::ParseCommand(buffer, request);

	if (!parseResult) {
		return false;
	}

	if (request.Timestamp != _keepAliveTimestamp) {
		return false;
	}

	_keepAliveTimestamp = 0;
	return true;
}

void ClientSession::SendKeepAlive()
{
	_keepAliveTimestamp = GetUnixTime();

	CommandKeepAlive::Command request;
	request.Timestamp = _keepAliveTimestamp;

	_protocol->Send(CommandKeepAlive::BuildCommand(request), 0);
}
