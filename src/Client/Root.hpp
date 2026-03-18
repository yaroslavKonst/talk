#ifndef _ROOT_HPP
#define _ROOT_HPP

#include "Processors.hpp"
#include "Config.hpp"
#include "../Common/IniFile.hpp"
#include "../Common/EventDispatcher.hpp"

struct Root
{
	NetworkEventProcessor *Network;
	UIEventProcessor *Ui;
	EventDispatcher *Dispatcher;
	Config *Conf;

	const uint8_t *PrivateKey;
	const uint8_t *PublicKey;

	Root();
};

#endif
