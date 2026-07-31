#include "Resolver.hpp"

#include <unistd.h>
#include <errno.h>

#include "Exception.hpp"
#include "Log.hpp"

Resolver::Resolver(EventDispatcher *dispatcher)
{
	_dispatcher = dispatcher;
	_resolverUser = nullptr;
	_addrinfo = nullptr;

	_fd[0] = -1;
	_fd[1] = -1;

	_status = EAI_AGAIN;
}

Resolver::~Resolver()
{
	_dispatcher->UnregisterQuantProcessor(this);

	_resolverUser = nullptr;

	try {
		if (_fd[0] != -1) {
			ProcessRead();
		}

		Clear();
	} catch (const Exception &ex) {
		Log("Resolver", ex.What());
	}
}

void Resolver::SetResolverUser(ResolverUser *user)
{
	_resolverUser = user;
}

void Resolver::Resolve(String host, String service, int socketType)
{
	struct addrinfo info;
	memset(&info, 0, sizeof(info));

	info.ai_family = AF_UNSPEC;
	info.ai_socktype = socketType;
	info.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG;

	_status = getaddrinfo(host.CStr(), service.CStr(), &info, &_addrinfo);

	if (_status) {
		_addrinfo = nullptr;
	}
}

void Resolver::RequestResolve(String host, String service, int socketType)
{
	if (_fd[0] != -1) {
		THROW("Resolver is busy.");
	}

	ThreadFunctionParams *params = new ThreadFunctionParams;
	params->Object = this;
	params->HostName = new char[host.Length() + 1];
	params->ServiceName = new char[service.Length() + 1];
	params->SocketType = socketType;

	memcpy(params->HostName, host.CStr(), host.Length() + 1);
	memcpy(params->ServiceName, service.CStr(), service.Length() + 1);

	int res = pipe(_fd);

	if (res) {
		delete params;
		THROW("Failed to create pipe.");
	}

	res = pthread_create(&_threadId, nullptr, ThreadFunction, params);

	if (res) {
		close(_fd[0]);
		close(_fd[1]);

		_fd[0] = -1;
		_fd[1] = -1;

		delete params;
		THROW("Failed to start getaddrinfo thread.");
	}

	_dispatcher->RegisterDescriptorProcessor(this);
}

int Resolver::GetResolveStatus()
{
	if (_fd[0] != -1) {
		THROW("Resolver is busy.");
	}

	return _status;
}

struct addrinfo *Resolver::GetResolveResult()
{
	if (_fd[0] != -1) {
		THROW("Resolver is busy.");
	}

	return _addrinfo;
}

void Resolver::Clear()
{
	if (_fd[0] != -1) {
		THROW("Resolver is busy.");
	}

	if (_addrinfo) {
		freeaddrinfo(_addrinfo);
		_addrinfo = nullptr;
	}

	_status = EAI_AGAIN;
}

int Resolver::GetDescriptor()
{
	return _fd[0];
}

bool Resolver::RequestRead()
{
	return true;
}

bool Resolver::RequestWrite()
{
	return false;
}

void Resolver::ProcessRead()
{
	_dispatcher->UnregisterDescriptorProcessor(this);

	char c;

	for (;;) {
		int res = read(_fd[0], &c, 1);

		if (res == 1) {
			break;
		}

		if (res == -1 && errno == EINTR) {
			continue;
		}

		THROW("Invalid read size in resolver.");

	}

	close(_fd[0]);
	close(_fd[1]);
	_fd[0] = -1;
	_fd[1] = -1;

	int res = pthread_join(_threadId, nullptr);

	if (res) {
		THROW("Failed to join thread.");
	}

	_dispatcher->RegisterQuantProcessor(this);
}

void Resolver::ProcessWrite()
{
	THROW("This method must never be called.");
}

void Resolver::ProcessQuant()
{
	if (_resolverUser) {
		_resolverUser->ResolveCompleted();
	}
}

void *Resolver::ThreadFunction(void *data)
{
	ThreadFunctionParams *params = static_cast<ThreadFunctionParams*>(data);

	params->Object->Resolve(
		params->HostName,
		params->ServiceName,
		params->SocketType);

	char c = 0;
	write(params->Object->_fd[1], &c, 1);

	delete params;
	return nullptr;
}
