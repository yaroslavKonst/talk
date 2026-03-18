#ifndef _CONTROL_SOCKET_HPP
#define _CONTROL_SOCKET_HPP

#include "UserDB.hpp"
#include "ControlSession.hpp"
#include "../Common/EventDispatcher.hpp"

class ControlSocket :
	public DescriptorEventProcessor,
	public QuantEventProcessor,
	public ControlSessionStorage
{
public:
	ControlSocket(UserDB *users, EventDispatcher *dispatcher);
	~ControlSocket();

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

	void ProcessQuant() override;

	void AddSession(int fd);
	void MarkSessionForRemoval(ControlSession *session) override;

private:
	int _socketFd;
	UserDB *_users;
	EventDispatcher *_dispatcher;

	struct ControlNode
	{
		ControlNode *Next;
		ControlSession *Session;
		bool Remove;
	};

	ControlNode *_controlSessions;
	bool _timeQuantRequested;
};

#endif
