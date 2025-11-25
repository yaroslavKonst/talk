#ifndef _CONTROL_SESSION_HPP
#define _CONTROL_SESSION_HPP

#include "Session.hpp"
#include "../Server/UserDB.hpp"

struct ControlSession : public Session
{
	ControlSession();

	UserDB *Users;
	bool *Work;

	const uint8_t *PublicKey;

	bool Process() override;
	bool TimePassed() override;

	void SendResponse(int32_t value, const CowBuffer<uint8_t> data);

	void ProcessShutdownCommand();
	void ProcessGetPublicKeyCommand();
	void ProcessAddUserCommand(const CowBuffer<uint8_t> message);
	void ProcessRemoveUserCommand(const CowBuffer<uint8_t> message);
	void ProcessListUsersCommand();

	void ProcessUnknownCommand();
};

#endif
