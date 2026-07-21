#ifndef _RESOLVER_HPP
#define _RESOLVER_HPP

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>

#include "MyString.hpp"
#include "EventDispatcher.hpp"

class ResolverUser
{
public:
	virtual ~ResolverUser()
	{ }

	virtual void ResolveCompleted() = 0;
};

class Resolver : public DescriptorEventProcessor
{
public:
	Resolver(EventDispatcher *dispatcher);
	~Resolver();

	void SetResolverUser(ResolverUser *user);

	void Resolve(String host, String service, int socketType);
	void RequestResolve(String host, String service, int socketType);
	int GetResolveStatus();
	struct addrinfo *GetResolveResult();
	void Clear();

	int GetDescriptor() override;
	bool RequestRead() override;
	bool RequestWrite() override;
	void ProcessRead() override;
	void ProcessWrite() override;

private:
	EventDispatcher *_dispatcher;
	ResolverUser *_resolverUser;

	struct addrinfo *_addrinfo;

	int _status;
	int _fd[2];

	struct ThreadFunctionParams
	{
		Resolver *Object;
		char *HostName;
		char *ServiceName;
		int SocketType;

		~ThreadFunctionParams()
		{
			delete[] HostName;
			delete[] ServiceName;
		}
	};

	pthread_t _threadId;
	static void *ThreadFunction(void *data);
};

#endif
