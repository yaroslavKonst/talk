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

Resolver::RequestA::RequestA()
{
	T = Type::A;
}

void Resolver::RequestA::Detach()
{
	DNSName.Detach();
}

Resolver::RequestAAAA::RequestAAAA()
{
	T = Type::AAAA;
}

void Resolver::RequestAAAA::Detach()
{
	DNSName.Detach();
}

Resolver::RequestRDNS::RequestRDNS()
{
	T = Type::RDNS;
}

void Resolver::RequestRDNS::Detach()
{
	ResultName.Detach();
}

Resolver::RequestPTR::RequestPTR()
{
	T = Type::PTR;
}

void Resolver::RequestPTR::Detach()
{
	DNSName.Detach();
	ResultName.Detach();
}

Resolver::RequestTXT::RequestTXT()
{
	T = Type::TXT;
}

void Resolver::RequestTXT::Detach()
{
	DNSName.Detach();
	Result.Detach();
}

Resolver::RequestSRV::RequestSRV()
{
	T = Type::SRV;
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
		ResolverLog(ex.What());
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

IPAddress Resolver::ResolveA(String dnsName)
{
	ResolverLog("Resolve A: " + dnsName + ".");
	CowBuffer<unsigned char> response = RunQuery(dnsName, C_IN, T_A);
	ResolverLog("Parsing.");

	if (!response.Size()) {
		_status = 1;
		return IPAddress();
	}

	ns_msg handle;
	int res = ns_initparse(response.Pointer(), response.Size(), &handle);

	if (res) {
		_status = 1;
		return IPAddress();
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
			IPAddress ipv4;
			ipv4.Type = IPAddress::AddressType::IPv4;
			memcpy(
				&ipv4.Address.IPv4,
				ns_rr_rdata(rr),
				sizeof(uint32_t));
			_status = 0;

			ResolverLog("Result: " + ipv4.ToString() + ".");
			return ipv4;
		}
	}

	_status = 1;
	return IPAddress();
}

IPAddress Resolver::ResolveAAAA(String dnsName)
{
	ResolverLog("Resolve AAAA: " + dnsName + ".");
	CowBuffer<unsigned char> response = RunQuery(dnsName, C_IN, T_AAAA);
	ResolverLog("Parsing.");

	if (!response.Size()) {
		_status = 1;
		return IPAddress();
	}

	ns_msg handle;
	int res = ns_initparse(response.Pointer(), response.Size(), &handle);

	if (res) {
		_status = 1;
		return IPAddress();
	}

	int entryCount = ns_msg_count(handle, ns_s_an);

	for (int i = 0; i < entryCount; i++) {
		ns_rr rr;

		if (ns_parserr(&handle, ns_s_an, i, &rr)) {
			continue;
		}

		bool isRequiredEntry =
			ns_rr_type(rr) == ns_t_aaaa &&
			ns_rr_rdlen(rr) == 16;

		if (isRequiredEntry) {
			IPAddress ipv6;
			ipv6.Type = IPAddress::AddressType::IPv6;
			memcpy(
				ipv6.Address.IPv6,
				ns_rr_rdata(rr),
				16);
			_status = 0;

			ResolverLog("Result: " + ipv6.ToString() + ".");
			return ipv6;
		}
	}

	_status = 1;
	return IPAddress();
}

String Resolver::ResolveRDNS(IPAddress ip)
{
	String dnsName;

	if (ip.Type == IPAddress::AddressType::IPv4) {
		uint8_t bytes[sizeof(uint32_t)];

		memcpy(bytes, &ip.Address.IPv4, sizeof(uint32_t));

		for (int i = sizeof(uint32_t) - 1; i >= 0; i--) {
			dnsName += ToString(bytes[i]) + ".";
		}

		dnsName += "in-addr.arpa";
	} else {
		uint8_t bytes[16];

		memcpy(bytes, ip.Address.IPv6, 16);

		for (int i = 15; i >= 0; i--) {
			uint8_t octet = bytes[i];

			uint8_t lp = octet & 0xf;
			uint8_t up = octet >> 4;

			if (lp < 10) {
				dnsName += char(lp + '0');
			} else {
				dnsName += char(lp + 'a' - 10);
			}

			dnsName += '.';

			if (up < 10) {
				dnsName += char(up + '0');
			} else {
				dnsName += char(up + 'a' - 10);
			}

			dnsName += '.';
		}

		dnsName += "ip6.arpa";
	}

	return ResolvePTR(dnsName);
}

String Resolver::ResolvePTR(String dnsName)
{
	ResolverLog("Resolve PTR: " + dnsName + ".");
	CowBuffer<unsigned char> response = RunQuery(dnsName, C_IN, T_PTR);
	ResolverLog("Parsing.");

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
			ResolverLog("Result: " + result + ".");
			return result;
		}
	}

	_status = 1;
	return "";
}

String Resolver::ResolveTXT(String dnsName)
{
	ResolverLog("Resolve TXT: " + dnsName + ".");
	CowBuffer<unsigned char> response = RunQuery(dnsName, C_IN, T_TXT);
	ResolverLog("Parsing.");

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

	bool needDelimiter = false;

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

				if (needDelimiter) {
					resultString += '\n';
					needDelimiter = false;
				}

				resultString += part;
				curr += len;
			}

			needDelimiter = true;
		}
	}

	_status = !hasEntry;
	ResolverLog("Result: " + resultString + ".");
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

	ResolverLog("Resolve SRV: " + query + ".");
	CowBuffer<unsigned char> response = RunQuery(query, C_IN, T_SRV);
	ResolverLog("Parsing.");

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
	ResolverLog("Result: " + ToString((long)firstEntry) + ".");
	return firstEntry;
}

void Resolver::StartAsyncResolve(CowBuffer<RequestBase*> requests)
{
	if (_fd[0] != -1) {
		THROW("Resolver is busy.");
	}

	ResolverLog("Starting async resolve.");

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

	ResolverLog("Async resolve started.");
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
	ResolverLog("Run query.");
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

	ResolverLog("Thread started work.");

	for (int i = 0; i < params->RequestCount; i++) {
		ResolverLog("Thread index: " + ToString(i) + ".");
		RequestBase *r = params->Requests[i];

		if (r->T == RequestBase::Type::A) {
			ResolverLog("Thread type: A.");
			RequestA *req = static_cast<RequestA*>(r);

			req->ResultIP = params->Object->ResolveA(
				req->DNSName);
			req->Status = params->Object->GetResolveStatus();
		} else if (r->T == RequestBase::Type::AAAA) {
			ResolverLog("Thread type: AAAA.");
			RequestAAAA *req = static_cast<RequestAAAA*>(r);

			req->ResultIP = params->Object->ResolveAAAA(
				req->DNSName);
			req->Status = params->Object->GetResolveStatus();
		} else if (r->T == RequestBase::Type::RDNS) {
			ResolverLog("Thread type: rDNS.");
			RequestRDNS *req = static_cast<RequestRDNS*>(r);

			req->ResultName = params->Object->ResolveRDNS(
				req->IP);
			req->Status = params->Object->GetResolveStatus();
		} else if (r->T == RequestBase::Type::PTR) {
			ResolverLog("Thread type: PTR.");
			RequestPTR *req = static_cast<RequestPTR*>(r);

			req->ResultName = params->Object->ResolvePTR(
				req->DNSName);
			req->Status = params->Object->GetResolveStatus();
		} else if (r->T == RequestBase::Type::TXT) {
			ResolverLog("Thread type: TXT.");
			RequestTXT *req = static_cast<RequestTXT*>(r);

			req->Result = params->Object->ResolveTXT(
				req->DNSName);
			req->Status = params->Object->GetResolveStatus();
		} else if (r->T == RequestBase::Type::SRV) {
			ResolverLog("Thread type: SRV.");
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

void Resolver::ResolverLog(String message)
{
	Log(LogLevel::Debug, "Resolver", message);
}
