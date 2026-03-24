#ifndef _CONTROL_SESSION_HPP
#define _CONTROL_SESSION_HPP

#include "UserDB.hpp"
#include "../Common/EventDispatcher.hpp"
#include "../Common/StreamReader.hpp"
#include "../Common/StreamWriter.hpp"
#include "../Common/MyString.hpp"

class ControlSessionStorage;

class ControlSession :
	public DescriptorEventProcessor,
	public TimeEventProcessor
{
public:
	ControlSession(
		int fd,
		UserDB *users,
		ControlSessionStorage *storage,
		EventDispatcher *dispatcher);
	~ControlSession();

	int GetDescriptor() override;
	bool RequestRead() override;
	bool RequestWrite() override;
	void ProcessRead() override;
	void ProcessWrite() override;

	void ProcessTimeEvent() override;

private:
	int _fd;
	UserDB *_users;
	EventDispatcher *_dispatcher;
	ControlSessionStorage *_storage;

	StreamReader *_reader;
	StreamWriter *_writer;

	uint64_t _requestSize;

	void Process(const CowBuffer<uint8_t> buffer);
	void SendResponse(CowBuffer<uint8_t> response);

	void ProcessUnknownCommand(int32_t command);
	void ProcessShutdownCommand();
	void ProcessGetKeyCommand();
	void ProcessReloadConfigCommand();
	void ProcessAddUserCommand(CowBuffer<uint8_t> buffer);
	void ProcessRemoveUserCommand(CowBuffer<uint8_t> buffer);
	void ProcessListUsersCommand(CowBuffer<uint8_t> buffer);
};

class ControlSessionStorage
{
public:
	virtual ~ControlSessionStorage()
	{ }

	virtual void MarkSessionForRemoval(ControlSession *session) = 0;
	virtual const uint8_t *GetPublicKey() = 0;
	virtual void ReloadConfig() = 0;
};

#endif
