#include "ControlSession.hpp"

#include <unistd.h>
#include <sys/socket.h>

#include "../Protocol/ControlParser.hpp"
#include "../Message/Message.hpp"
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
	EventDispatcher *dispatcher,
	FailBan *failBan)
{
	SetInterval(10000);
	SetTimestamp(GetMonotonicMillisecondTime());

	_fd = fd;
	_users = users;
	_dispatcher = dispatcher;
	_failBan = failBan;
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

	SetTimestamp(GetMonotonicMillisecondTime());

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

	SetTimestamp(GetMonotonicMillisecondTime());

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
	case COMMAND_RELOAD_CONFIG:
		ProcessReloadConfigCommand();
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
	case COMMAND_FAILBAN_LIST_BANNED:
		ProcessFailBanListBannedCommand();
		break;
	case COMMAND_FAILBAN_BAN:
		ProcessFailBanBanCommand(buffer);
		break;
	case COMMAND_FAILBAN_UNBAN:
		ProcessFailBanUnbanCommand(buffer);
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
	ControlLog("Received unknown command with code " +
		ToString(command) + ".");
	int32_t code = ERROR_UNKNOWN_COMMAND;

	CowBuffer<uint8_t> response(sizeof(code));
	*response.SwitchType<int32_t>() = code;

	SendResponse(response);
}

void ControlSession::ProcessShutdownCommand()
{
	ControlLog("Shutdown is requested.");
	_dispatcher->Stop();
}

void ControlSession::ProcessGetKeyCommand()
{
	ControlLog("Public key is requested.");

	CowBuffer<uint8_t> code(sizeof(int32_t));
	*code.SwitchType<int32_t>() = OK;

	CowBuffer<uint8_t> key(Crypto::X25519::KEY_SIZE);
	memcpy(
		key.Pointer(),
		_storage->GetPublicKey().Key,
		Crypto::X25519::KEY_SIZE);

	SendResponse(code.Concat(key));
}

void ControlSession::ProcessReloadConfigCommand()
{
	ControlLog("Reloading configuration.");

	CowBuffer<uint8_t> code(sizeof(int32_t));

	try {
		_storage->ReloadConfig();
		*code.SwitchType<int32_t>() = OK;
		ControlLog("Reloaded successfully.");
	} catch (Exception &ex) {
		*code.SwitchType<int32_t>() = ERROR;
		ControlLog("Reloading error: " + ex.Message());
	}

	SendResponse(code);
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

	String fullNewUserName = request.Name + "@" + _storage->GetHostName();

	if (!Message::VerifyFullUserName(fullNewUserName)) {
		ControlLog("Attempt to add invalid user name " + request.Name +
			".");
		response.Code = ERROR_INVALID_USER;
	} else if (_users->HasUser(request.Name)) {
		ControlLog("Attempt to add existing user " + request.Name +
			".");
		response.Code = ERROR_USER_EXISTS;
	} else {
		response.Code = OK;
		ControlLog("Adding new user " + request.Name +
			" with key " +
			DataToHex(request.Key.Key, Crypto::X25519::KEY_SIZE) +
			".");
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
		bool result = _users->RemoveUser(request.Name);

		if (!result) {
			response.Code = ERROR_USER_BUSY;
		}
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
		}
	}

	SendResponse(CommandListUsers::BuildResponse(response));
}

void ControlSession::ProcessFailBanListBannedCommand()
{
	CommandFailBanListBanned::Response response;

	response.Code = OK;
	response.BannedIPList = _failBan->ListBanned();

	SendResponse(CommandFailBanListBanned::BuildResponse(response));
}

void ControlSession::ProcessFailBanBanCommand(CowBuffer<uint8_t> buffer)
{
	CommandFailBanBan::Request request;
	bool parseResult = CommandFailBanBan::ParseRequest(buffer, request);

	if (!parseResult) {
		_storage->MarkSessionForRemoval(this);
		return;
	}

	bool result = _failBan->Ban(request.IP);

	CommandFailBanBan::Response response;

	if (result) {
		response.Code = OK;
	} else {
		response.Code = ERROR_INVALID_IP;
	}

	SendResponse(CommandFailBanBan::BuildResponse(response));
}

void ControlSession::ProcessFailBanUnbanCommand(CowBuffer<uint8_t> buffer)
{
	CommandFailBanUnban::Request request;
	bool parseResult = CommandFailBanUnban::ParseRequest(buffer, request);

	if (!parseResult) {
		_storage->MarkSessionForRemoval(this);
		return;
	}

	bool result = _failBan->Unban(request.IP);

	CommandFailBanUnban::Response response;

	if (result) {
		response.Code = OK;
	} else {
		response.Code = ERROR_INVALID_IP;
	}

	SendResponse(CommandFailBanUnban::BuildResponse(response));
}

void ControlSession::ControlLog(String message)
{
	Log(LogLevel::Warning, "Control", message);
}
