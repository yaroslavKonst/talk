#include "Resolver.hpp"

#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include "Exception.hpp"
#include "Log.hpp"

Resolver::SRVResult::SRVResult()
{
	Next = nullptr;
}

Resolver::SRVResult::~SRVResult()
{
	if (Next) {
		delete Next;
		Next = nullptr;
	}
}

Resolver::RequestGetAddrInfo::RequestGetAddrInfo()
{
	AddrInfo = nullptr;
}

Resolver::RequestGetAddrInfo::~RequestGetAddrInfo()
{
	if (AddrInfo) {
		FreeAddrInfo(AddrInfo);
		AddrInfo = nullptr;
	}
}

void Resolver::RequestGetAddrInfo::Detach()
{
	Host.Detach();
	Service.Detach();
}

void Resolver::RequestA::Detach()
{
	DNSName.Detach();
}

void Resolver::RequestRDNS::Detach()
{
	ResultName.Detach();
}

void Resolver::RequestPTR::Detach()
{
	DNSName.Detach();
	ResultName.Detach();
}

void Resolver::RequestTXT::Detach()
{
	DNSName.Detach();
	Result.Detach();
}

Resolver::RequestSRV::RequestSRV()
{
	Result = nullptr;
}

Resolver::RequestSRV::~RequestSRV()
{
	if (Result) {
		delete Result;
		Result = nullptr;
	}
}

void Resolver::RequestSRV::Detach()
{
	DNSName.Detach();
	ServiceName.Detach();
}

void Resolver::FreeAddrInfo(struct addrinfo *addr)
{
	if (addr) {
		freeaddrinfo(addr);
	}
}

Resolver::Resolver(EventDispatcher *dispatcher)
{
	_dispatcher = dispatcher;
	_resolverUser = nullptr;

	_fd[0] = -1;
	_fd[1] = -1;

	_status = EAI_AGAIN;

	_hasOwnership = false;
}

Resolver::~Resolver()
{
	_dispatcher->UnregisterQuantProcessor(this);

	_resolverUser = nullptr;

	try {
		if (_fd[0] != -1) {
			ProcessRead();
			_dispatcher->UnregisterQuantProcessor(this);
		}
	} catch (const Exception &ex) {
		Log("Resolver", ex.What());
	}
}

void Resolver::SetResolverUser(ResolverUser *user)
{
	_resolverUser = user;
}

int Resolver::GetResolveStatus()
{
	return _status;
}

void Resolver::PassOwnership()
{
	_hasOwnership = true;
}

struct addrinfo *Resolver::ResolveGetAddrInfo(
	String host,
	String service,
	int socketType)
{
	struct addrinfo info;
	memset(&info, 0, sizeof(info));

	info.ai_family = AF_UNSPEC;
	info.ai_socktype = socketType;
	info.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG;

	struct addrinfo *addr;

	_status = getaddrinfo(host.CStr(), service.CStr(), &info, &addr);

	if (_status) {
		return nullptr;
	}

	return addr;
}

uint32_t Resolver::ResolveA(String dnsName)
{
	CowBuffer<unsigned char> response = RunQuery(dnsName, C_IN, T_A);

	if (!response.Size()) {
		_status = 1;
		return 0;
	}

	ns_msg handle;
	int res = ns_initparse(response.Pointer(), response.Size(), &handle);

	if (res) {
		_status = 1;
		return 0;
	}

	int entryCount = ns_msg_count(handle, ns_s_an);

	for (int i = 0; i < entryCount; i++) {
		ns_rr rr;

		if (ns_parserr(&handle, ns_s_an, i, &rr)) {
			continue;
		}

		bool isRequiredEntry =
			ns_rr_type(rr) == ns_t_a &&
			ns_rr_rdlen(rr) == sizeof(uint32_t);

		if (isRequiredEntry) {
			uint32_t ipv4;
			memcpy(&ipv4, ns_rr_rdata(rr), sizeof(ipv4));
			_status = 0;
			return ipv4;
		}
	}

	_status = 1;
	return 0;
}

String Resolver::ResolveRDNS(uint32_t ipv4)
{
	String dnsName;
	uint8_t bytes[sizeof(ipv4)];

	memcpy(bytes, &ipv4, sizeof(ipv4));

	for (int i = sizeof(ipv4) - 1; i >= 0; i--) {
		dnsName += ToString(bytes[i]) + ".";
	}

	dnsName += "in-addr.arpa";

	return ResolvePTR(dnsName);
}

String Resolver::ResolvePTR(String dnsName)
{
	CowBuffer<unsigned char> response = RunQuery(dnsName, C_IN, T_PTR);

	if (!response.Size()) {
		_status = 1;
		return "";
	}

	ns_msg handle;
	int res = ns_initparse(response.Pointer(), response.Size(), &handle);

	if (res) {
		_status = 1;
		return "";
	}

	int entryCount = ns_msg_count(handle, ns_s_an);

	for (int i = 0; i < entryCount; i++) {
		ns_rr rr;

		if (ns_parserr(&handle, ns_s_an, i, &rr)) {
			continue;
		}

		bool isRequiredEntry =
			ns_rr_type(rr) == ns_t_ptr;

		if (isRequiredEntry) {
			char name[NS_MAXDNAME];

			int len = ns_name_uncompress(
				ns_msg_base(handle),
				ns_msg_end(handle),
				ns_rr_rdata(rr),
				name,
				sizeof(name));

			if (len < 0) {
				continue;
			}

			String result(name);
			_status = 0;
			return result;
		}
	}

	_status = 1;
	return "";
}

String Resolver::ResolveTXT(String dnsName)
{
	CowBuffer<unsigned char> response = RunQuery(dnsName, C_IN, T_TXT);

	if (!response.Size()) {
		_status = 1;
		return "";
	}

	ns_msg handle;
	int res = ns_initparse(response.Pointer(), response.Size(), &handle);

	if (res) {
		_status = 1;
		return "";
	}

	int entryCount = ns_msg_count(handle, ns_s_an);

	String resultString;
	bool hasEntry = false;

	for (int i = 0; i < entryCount; i++) {
		ns_rr rr;

		if (ns_parserr(&handle, ns_s_an, i, &rr)) {
			continue;
		}

		bool isRequiredEntry =
			ns_rr_type(rr) == ns_t_txt;

		if (isRequiredEntry) {
			hasEntry = true;

			const uint8_t *curr = ns_rr_rdata(rr);
			const uint8_t *end = curr + ns_rr_rdlen(rr);

			while (curr < end) {
				int len = *curr;
				++curr;

				if (curr + len > end) {
					break;
				}

				String part((const char*)curr, len);
				resultString += part;
				curr += len;
			}
		}
	}

	_status = !hasEntry;
	return resultString;
}

Resolver::SRVResult *Resolver::ResolveSRV(
	String dnsName,
	String serviceName,
	bool tcp)
{
	String query =
		"_" + serviceName +
		"._" + (tcp ? "tcp" : "udp") +
		"." + dnsName;

	CowBuffer<unsigned char> response = RunQuery(query, C_IN, T_SRV);

	if (!response.Size()) {
		_status = 1;
		return nullptr;
	}

	ns_msg handle;
	int res = ns_initparse(response.Pointer(), response.Size(), &handle);

	if (res) {
		_status = 1;
		return nullptr;
	}

	SRVResult *firstEntry = nullptr;

	int entryCount = ns_msg_count(handle, ns_s_an);

	for (int i = 0; i < entryCount; i++) {
		ns_rr rr;

		if (ns_parserr(&handle, ns_s_an, i, &rr)) {
			continue;
		}

		bool isRequiredEntry =
			ns_rr_type(rr) == ns_t_srv;

		if (isRequiredEntry) {
			if (ns_rr_rdlen(rr) < sizeof(uint16_t) * 3 + 1) {
				continue;
			}

			SRVResult *entry = new SRVResult;

			const uint8_t *curr = ns_rr_rdata(rr);

			entry->Priority = ns_get16(curr);
			curr += sizeof(uint16_t);
			entry->Weight = ns_get16(curr);
			curr += sizeof(uint16_t);
			// Store port in network byte order.
			entry->Port = htons((uint16_t)ns_get16(curr));
			curr += sizeof(uint16_t);

			char name[NS_MAXDNAME];

			int len = ns_name_uncompress(
				ns_msg_base(handle),
				ns_msg_end(handle),
				curr,
				name,
				sizeof(name));

			if (len < 0) {
				delete entry;
				continue;
			}

			entry->Target = String(name);

			if (!entry->Target.Length() || entry->Target == ".") {
				delete entry;
				continue;
			}

			SRVResult **node = &firstEntry;

			while (*node) {
				if ((*node)->Priority < entry->Priority) {
					node = &(*node)->Next;
				} else {
					entry->Next = *node;
					*node = entry;
					break;
				}
			}

			if (!*node) {
				*node = entry;
				continue;
			}
		}
	}

	_status = !firstEntry;
	return firstEntry;
}

void Resolver::StartAsyncResolve(CowBuffer<RequestBase*> requests)
{
	if (_fd[0] != -1) {
		THROW("Resolver is busy.");
	}

	_asyncRequests = requests;

	for (uint32_t i = 0; i < _asyncRequests.Size(); i++) {
		_asyncRequests[i]->Detach();
	}

	ThreadFunctionParams *params = new ThreadFunctionParams;
	params->Object = this;
	params->Requests = _asyncRequests.Pointer();
	params->RequestCount = _asyncRequests.Size();

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

	if (_hasOwnership) {
		for (uint32_t i = 0; i < _asyncRequests.Size(); i++) {
			delete _asyncRequests[i];
		}
	}

	_asyncRequests = CowBuffer<RequestBase*>();

	if (!_hasOwnership) {
		_dispatcher->RegisterQuantProcessor(this);
	}

	_hasOwnership = false;
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

CowBuffer<unsigned char> Resolver::RunQuery(String dnsName, int cls, int type)
{
	struct __res_state state;
	memset(&state, 0, sizeof(state));

	int res = res_ninit(&state);

	if (res) {
		return CowBuffer<unsigned char>();
	}

	int responseSize = 4096;

	CowBuffer<unsigned char> response(responseSize);
	int resSize;

	for (;;) {
		resSize = res_nquery(
			&state,
			dnsName.CStr(),
			cls,
			type,
			response.Pointer(),
			response.Size());

		if (resSize == -1) {
			res_nclose(&state);
			return CowBuffer<unsigned char>();
		}

		if (resSize > (int)response.Size()) {
			responseSize *= 4;

			if (responseSize > 1024 * 1024) {
				res_nclose(&state);
				return CowBuffer<unsigned char>();
			}

			response = CowBuffer<unsigned char>(responseSize);
			continue;
		}

		ns_msg handle;
		int parseRes = ns_initparse(
			response.Pointer(),
			resSize,
			&handle);

		if (parseRes || ns_msg_getflag(handle, ns_f_tc)) {
			res_nclose(&state);
			return CowBuffer<unsigned char>();
		}

		break;
	}

	res_nclose(&state);

	response.Resize(resSize);
	return response;
}

void *Resolver::ThreadFunction(void *data)
{
	ThreadFunctionParams *params = static_cast<ThreadFunctionParams*>(data);

	for (int i = 0; i < params->RequestCount; i++) {
		RequestBase *r = params->Requests[i];

		if (r->T == RequestBase::Type::GetAddrInfo) {
			RequestGetAddrInfo *req =
				static_cast<RequestGetAddrInfo*>(r);

			req->AddrInfo = params->Object->ResolveGetAddrInfo(
				req->Host,
				req->Service,
				req->SocketType);
			req->Status = params->Object->GetResolveStatus();
		} else if (r->T == RequestBase::Type::A) {
			RequestA *req = static_cast<RequestA*>(r);

			req->ResultIPv4 = params->Object->ResolveA(
				req->DNSName);
			req->Status = params->Object->GetResolveStatus();
		} else if (r->T == RequestBase::Type::RDNS) {
			RequestRDNS *req = static_cast<RequestRDNS*>(r);

			req->ResultName = params->Object->ResolveRDNS(
				req->IPv4);
			req->Status = params->Object->GetResolveStatus();
		} else if (r->T == RequestBase::Type::PTR) {
			RequestPTR *req = static_cast<RequestPTR*>(r);

			req->ResultName = params->Object->ResolvePTR(
				req->DNSName);
			req->Status = params->Object->GetResolveStatus();
		} else if (r->T == RequestBase::Type::TXT) {
			RequestTXT *req = static_cast<RequestTXT*>(r);

			req->Result = params->Object->ResolveTXT(
				req->DNSName);
			req->Status = params->Object->GetResolveStatus();
		} else if (r->T == RequestBase::Type::SRV) {
			RequestSRV *req = static_cast<RequestSRV*>(r);

			req->Result = params->Object->ResolveSRV(
				req->DNSName,
				req->ServiceName,
				req->TCP);
			req->Status = params->Object->GetResolveStatus();
		}
	}

	char c = 0;
	write(params->Object->_fd[1], &c, 1);

	delete params;
	return nullptr;
}
