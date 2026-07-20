#ifndef _ROOT_HPP
#define _ROOT_HPP

#include "Processors.hpp"
#include "Config.hpp"
#include "../Common/IniFile.hpp"
#include "../Common/EventDispatcher.hpp"
#include "../Crypto/Crypto.hpp"

struct Root
{
	NetworkEventProcessor *Network;
	UIEventProcessor *Ui;
	EventDispatcher *Dispatcher;
	Config *Conf;
	MessageEventProcessor *Messages;

	const Crypto::X25519::PrivateKeyContainer *PrivateKey;
	const Crypto::X25519::PublicKeyContainer *PublicKey;

	Root();
};

#endif
