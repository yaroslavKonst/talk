#ifndef _LISTENING_SOCKET_HPP
#define _LISTENING_SOCKET_HPP

#include "UserDB.hpp"
#include "Config.hpp"
#include "../Common/EventDispatcher.hpp"

class ListeningSocket :
	public DescriptorEventProcessor,
	public ConfigUser
{
public:
	ListeningSocket(
		UserDB *users,
		EventDispatcher *dispatcher,
		Config *config);
	~ListeningSocket();

	void ReloadConfig() override;

	int GetDescriptor() override
	{
		return _socketFd;
	}

	bool RequestRead() override
	{
		return true;
	}

	bool RequestWrite() override
	{
		return false;
	}

	void ProcessRead() override;
	void ProcessWrite() override;

	void OpenSocket();
	void CloseSocket();

private:
	int _socketFd;
	UserDB *_users;
	EventDispatcher *_dispatcher;
	Config *_config;
};

#endif
