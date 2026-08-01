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
	ControlSocket(
		UserDB *users,
		EventDispatcher *dispatcher,
		Config *config,
		FailBan *failBan,
		const Crypto::X25519::PublicKeyContainer &publicKey);
	~ControlSocket();

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

	void ProcessQuant() override;

	void AddSession(int fd);
	void MarkSessionForRemoval(ControlSession *session) override;

	const Crypto::X25519::PublicKeyContainer &GetPublicKey() override
	{
		return _publicKey;
	}

	String GetHostName() override;

private:
	int _socketFd;
	UserDB *_users;
	EventDispatcher *_dispatcher;
	Config *_config;
	FailBan *_failBan;

	struct ControlNode
	{
		ControlNode *Next;
		ControlSession *Session;
		bool Remove;
	};

	ControlNode *_controlSessions;
	bool _timeQuantRequested;

	const Crypto::X25519::PublicKeyContainer &_publicKey;
};

#endif
