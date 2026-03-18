#ifndef _NETWORK_HPP
#define _NETWORK_HPP

#include "Root.hpp"
#include "ClientHandshake.hpp"

class Network :
	public NetworkEventProcessor,
	public DescriptorEventProcessor,
	public TimeEventProcessor
{
public:
	Network(Root *root);
	~Network();

	int GetDescriptor() override
	{
		return _fd;
	}

	bool RequestRead() override;
	bool RequestWrite() override;
	void ProcessRead() override;
	void ProcessWrite() override;

	void ProcessTimeEvent() override;

	bool ConnectionActive() override;
	bool HandshakeActive() override;

	void StartConnection(int fd, const uint8_t *serverKey) override;

private:
	int _fd;
	Root *_root;

	ClientHandshake *_handshake;
	void *_session;

	void CloseConnection();
};

#endif
