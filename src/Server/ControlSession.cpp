#include "ControlSession.hpp"

#include <unistd.h>
#include <sys/socket.h>

#include "../Protocol/ControlParser.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/Exception.hpp"
#include "../Common/Log.hpp"
#include "../Common/Debug.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

ControlSession::ControlSession(
	int fd,
	ControlSessionStorage *storage,
	EventDispatcher *dispatcher)
{
	SetInterval(10);
	SetTimestamp(GetUnixTime());

	_fd = fd;
	_dispatcher = dispatcher;
	_storage = storage;

	_writer = nullptr;
	_reader = new StreamReader(_fd, sizeof(_requestSize));
	_requestSize = 0;

	_dispatcher->RegisterDescriptorProcessor(this);
	_dispatcher->RegisterTimeProcessor(this);
}

ControlSession::~ControlSession()
{
	_dispatcher->UnregisterDescriptorProcessor(this);
	_dispatcher->UnregisterTimeProcessor(this);

	if (_fd != -1) {
		shutdown(_fd, SHUT_RDWR);
		close(_fd);
		_fd = -1;
	}

	if (_reader) {
		delete _reader;
		_reader = nullptr;
	}

	if (_writer) {
		delete _writer;
		_writer = nullptr;
	}
}

int ControlSession::GetDescriptor()
{
	return _fd;
}

bool ControlSession::RequestRead()
{
	return _reader;
}

bool ControlSession::RequestWrite()
{
	return _writer;
}

void ControlSession::ProcessRead()
{
	if (!_reader) {
		THROW("Reader is null.");
	}

	SetTimestamp(GetUnixTime());

	bool readSuccess = _reader->Read();

	if (!readSuccess) {
		delete _reader;
		_reader = nullptr;
		_storage->MarkSessionForRemoval(this);
		return;
	}

	bool readEnd = _reader->ReadingEnd();

	if (!readEnd) {
		return;
	}

	CowBuffer<uint8_t> buffer = _reader->GetBuffer();

	delete _reader;
	_reader = nullptr;

	if (_writer) {
		_storage->MarkSessionForRemoval(this);
		return;
	}

	Process(buffer);
}

void ControlSession::ProcessWrite()
{
	if (!_writer) {
		THROW("Writer is null.");
	}

	SetTimestamp(GetUnixTime());

	bool writeSuccess = _writer->Write();

	if (!writeSuccess) {
		delete _writer;
		_writer = nullptr;
		_storage->MarkSessionForRemoval(this);
		return;
	}

	bool writeEnd = _writer->WritingEnd();

	if (!writeEnd) {
		return;
	}

	delete _writer;
	_writer = nullptr;
}

void ControlSession::ProcessTimeEvent()
{
	if (_reader) {
		delete _reader;
		_reader = nullptr;
	}

	if (_writer) {
		delete _writer;
		_writer = nullptr;
	}

	_storage->MarkSessionForRemoval(this);
}

void ControlSession::Process(const CowBuffer<uint8_t> buffer)
{
	if (_requestSize == 0) {
		if (buffer.Size() != sizeof(_requestSize)) {
			THROW("Invalid request size buffer size.");
		}

		_requestSize = *buffer.SwitchType<uint64_t>();

		if (_requestSize > 1024 * 1024 * 1024) {
			_storage->MarkSessionForRemoval(this);
			return;
		}

		_reader = new StreamReader(_fd, _requestSize);
		return;
	}

	if (buffer.Size() != _requestSize) {
		THROW("Invalid buffer size.");
	}

	_requestSize = 0;

	if (buffer.Size() < sizeof(int32_t)) {
		_storage->MarkSessionForRemoval(this);
		return;
	}

	int32_t command = *buffer.SwitchType<int32_t>();

	switch (command) {
		case COMMAND_SHUTDOWN:
			ProcessShutdownCommand();
			break;
		case COMMAND_GET_PUBLIC_KEY:
			ProcessGetKeyCommand();
			break;
		default:
			ProcessUnknownCommand(command);
			break;
	}

	_reader = new StreamReader(_fd, sizeof(_requestSize));
}

void ControlSession::SendResponse(CowBuffer<uint8_t> response)
{
	CowBuffer<uint8_t> result(sizeof(uint64_t));
	*result.SwitchType<uint64_t>() = response.Size();

	_writer = new StreamWriter(_fd, result.Concat(response));
}

void ControlSession::ProcessUnknownCommand(int32_t command)
{
	Log("Control: Received unknown command with code " +
		ToString(command) + ".");
	int32_t code = ERROR_UNKNOWN_COMMAND;

	CowBuffer<uint8_t> response(sizeof(code));
	*response.SwitchType<int32_t>() = code;

	SendResponse(response);
}

void ControlSession::ProcessShutdownCommand()
{
	Log("Control: Shutdown is requested.");
	_dispatcher->Stop();
}

void ControlSession::ProcessGetKeyCommand()
{
	Log("Control: Public key is requested.");

	CowBuffer<uint8_t> code(sizeof(int32_t));
	*code.SwitchType<int32_t>() = OK;

	CowBuffer<uint8_t> key(KEY_SIZE);
	memcpy(key.Pointer(), _storage->GetPublicKey(), KEY_SIZE);

	SendResponse(code.Concat(key));
}
