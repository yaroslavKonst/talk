#include "ControlSession.hpp"

#include <unistd.h>
#include <sys/socket.h>

#include "../Protocol/ControlParser.hpp"
#include "../Common/UnixTime.hpp"
#include "../Common/Exception.hpp"
#include "../Common/Log.hpp"
#include "../Common/Hex.hpp"
#include "../Common/Debug.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

ControlSession::ControlSession(
	int fd,
	UserDB *users,
	ControlSessionStorage *storage,
	EventDispatcher *dispatcher)
{
	SetInterval(10);
	SetTimestamp(GetUnixTime());

	_fd = fd;
	_users = users;
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

		if (_requestSize > 1024 * 1024 * 1024 || !_requestSize) {
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
	case COMMAND_ADD_USER:
		ProcessAddUserCommand(buffer);
		break;
	case COMMAND_REMOVE_USER:
		ProcessRemoveUserCommand(buffer);
		break;
	case COMMAND_LIST_USERS:
		ProcessListUsersCommand(buffer);
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

void ControlSession::ProcessAddUserCommand(CowBuffer<uint8_t> buffer)
{
	CommandAddUser::Request request;
	bool parseResult = CommandAddUser::ParseRequest(buffer, request);

	if (!parseResult) {
		_storage->MarkSessionForRemoval(this);
		return;
	}

	CommandAddUser::Response response;

	if (_users->HasUser(request.Name)) {
		Log("Control: Attempt to add existing user " + request.Name +
			".");
		response.Code = ERROR_USER_EXISTS;
	} else {
		response.Code = OK;
		Log("Control: Adding new user " + request.Name +
			" with key " + DataToHex(request.Key, KEY_SIZE) + ".");
		_users->AddUser(request.Name, request.Key);
	}

	SendResponse(CommandAddUser::BuildResponse(response));
}

void ControlSession::ProcessRemoveUserCommand(CowBuffer<uint8_t> buffer)
{
	CommandRemoveUser::Request request;
	bool parseResult = CommandRemoveUser::ParseRequest(buffer, request);

	if (!parseResult) {
		_storage->MarkSessionForRemoval(this);
		return;
	}

	CommandRemoveUser::Response response;

	if (!_users->HasUser(request.Name)) {
		response.Code = ERROR_INVALID_USER;
	} else {
		response.Code = OK;
		_users->RemoveUser(request.Name);
	}

	SendResponse(CommandRemoveUser::BuildResponse(response));
}

static bool ValidateFlags(int32_t flags)
{
	int32_t supportedFlags = CommandListUsers::ShowKeys;

	return !(flags & ~supportedFlags);
}

void ControlSession::ProcessListUsersCommand(CowBuffer<uint8_t> buffer)
{
	CommandListUsers::Request request;
	bool parseResult = CommandListUsers::ParseRequest(buffer, request);

	if (!parseResult) {
		_storage->MarkSessionForRemoval(this);
		return;
	}

	CommandListUsers::Response response;

	CowBuffer<String> userNames = _users->ListUsers();

	if (!ValidateFlags(request.Flags)) {
		response.Code = ERROR_UNSUPPORTED_OPTION;
		response.Flags = 0;
		SendResponse(CommandListUsers::BuildResponse(response));
		return;
	}

	response.Code = OK;
	response.Flags = request.Flags;
	response.Data = CowBuffer<CommandListUsers::Response::UserData>(
		userNames.Size());

	for (uint64_t i = 0; i < userNames.Size(); i++) {
		response.Data[i].Name = userNames[i];

		if (request.Flags & CommandListUsers::ShowKeys) {
			response.Data[i].Key =
				_users->GetUser(userNames[i])->GetPublicKey();
		} else {
			response.Data[i].Key = nullptr;
		}
	}

	SendResponse(CommandListUsers::BuildResponse(response));
}
