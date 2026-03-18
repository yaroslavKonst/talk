#ifndef _CONTROL_SESSION_HPP
#define _CONTROL_SESSION_HPP

#include "../Common/EventDispatcher.hpp"
#include "../Common/StreamReader.hpp"
#include "../Common/StreamWriter.hpp"

class ControlSessionStorage;

class ControlSession :
	public DescriptorEventProcessor,
	public TimeEventProcessor
{
public:
	ControlSession(
		int fd,
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
	EventDispatcher *_dispatcher;
	ControlSessionStorage *_storage;

	StreamReader *_reader;
	StreamWriter *_writer;

	uint64_t _requestSize;

	void Process(const CowBuffer<uint8_t> buffer);
	void SendResponse(CowBuffer<uint8_t> response);

	void ProcessUnknownCommand(int32_t command);
	void ProcessShutdownCommand();
};

class ControlSessionStorage
{
public:
	virtual ~ControlSessionStorage()
	{ }

	virtual void MarkSessionForRemoval(ControlSession *session) = 0;
};

#endif
