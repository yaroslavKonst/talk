#include "ClientSession.hpp"

#include "../Common/Exception.hpp"

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

bool ClientSession::ProcessInput(CowBuffer<uint8_t> buffer)
{
	THROW("Not implemented.");
}
