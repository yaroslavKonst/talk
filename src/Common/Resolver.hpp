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

class Resolver :
	public DescriptorEventProcessor,
	public QuantEventProcessor
{
public:
	struct SRVResult
	{
		int Priority;
		int Weight;
		uint16_t Port; // Stored in network byte order.
		String Target;

		SRVResult *Next;

		SRVResult();
		~SRVResult();
	};

	struct RequestBase
	{
		int Status;

		enum class Type
		{
			A,
			RDNS,
			PTR,
			TXT,
			SRV
		};

		Type T;

		virtual ~RequestBase()
		{ }

		virtual void Detach() = 0;
	};

	struct RequestA : public RequestBase
	{
		String DNSName;

		uint32_t ResultIPv4;

		RequestA();

		void Detach() override;
	};

	struct RequestRDNS : public RequestBase
	{
		uint32_t IPv4;

		String ResultName;

		RequestRDNS();

		void Detach() override;
	};

	struct RequestPTR : public RequestBase
	{
		String DNSName;

		String ResultName;

		RequestPTR();

		void Detach() override;
	};

	struct RequestTXT : public RequestBase
	{
		String DNSName;

		String Result;

		RequestTXT();

		void Detach() override;
	};

	struct RequestSRV : public RequestBase
	{
		String DNSName;
		String ServiceName;
		bool TCP;

		SRVResult *Result;

		RequestSRV();
		~RequestSRV();

		void Detach() override;
	};

	Resolver(EventDispatcher *dispatcher);
	~Resolver();

	void SetResolverUser(ResolverUser *user);
	int GetResolveStatus();

	void PassOwnership();

	uint32_t ResolveA(String dnsName);
	String ResolveRDNS(uint32_t ipv4);
	String ResolvePTR(String dnsName);
	String ResolveTXT(String dnsName);
	SRVResult *ResolveSRV(String dnsName, String serviceName, bool tcp);

	void StartAsyncResolve(CowBuffer<RequestBase*> requests);

	int GetDescriptor() override;
	bool RequestRead() override;
	bool RequestWrite() override;
	void ProcessRead() override;
	void ProcessWrite() override;

	void ProcessQuant() override;

private:
	EventDispatcher *_dispatcher;
	ResolverUser *_resolverUser;

	CowBuffer<RequestBase*> _asyncRequests;

	CowBuffer<unsigned char> RunQuery(String dnsName, int cls, int type);

	int _status;
	int _fd[2];

	bool _hasOwnership;

	struct ThreadFunctionParams
	{
		Resolver *Object;
		RequestBase **Requests;
		int RequestCount;
	};

	pthread_t _threadId;
	static void *ThreadFunction(void *data);

	static void ResolverLog(String message);
};

#endif
