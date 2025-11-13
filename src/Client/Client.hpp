#ifndef _CLIENT_HPP
#define _CLIENT_HPP

#include "UI.hpp"
#include "../Protocol/ClientSession.hpp"

class Client
{
public:
	Client();
	~Client();

	int Run();

private:
	ClientSession _session;

	UI _ui;
};

#endif
