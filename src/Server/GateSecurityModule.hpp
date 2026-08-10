#ifndef _GATE_SECURITY_MODULE_HPP
#define _GATE_SECURITY_MODULE_HPP

#include "../Common/Resolver.hpp"

class GateSecurityModule : public ResolverUser
{
public:
	GateSecurityModule(EventDispatcher *dispatcher);
	~GateSecurityModule();

	void SetUser(ResolverUser *user);

	bool Failure();

	void ResolveCompleted() override;

	void SetKnownPeerIP(uint32_t ipv4);
	void SetKnownPeerFullHostName(String fullHostName);
	void SetPeerReportedFullHostName(String fullHostName);

	String GetFullHostName();
	String GetSRVReportedServiceName();
	uint32_t GetDNSReportedIPv4();
	bool IsIPOnlyHost();
	void AcceptHostNameAsIP();

	bool NeedSRV();
	void RunSRV();

	bool NeedA();
	void RunA();

	bool NeedParams();
	void RunParams();

	void ValidateSyn(
		const CowBuffer<uint8_t> buffer,
		const CowBuffer<uint8_t> signature);

	void RunFullValidation();

private:
	ResolverUser *_user;
	bool _failure;

	Resolver _resolver;
	CowBuffer<Resolver::RequestBase*> _requests;

	bool _ipOnlyHostName;

	uint32_t _knownPeerIP;
	bool _knownPeerIPAssigned;

	// Pre SRV.
	String _knownPeerFullHostName;
	String _knownPeerHostName;
	String _knownPeerServiceName;

	uint32_t _dnsReportedPeerIP;
	bool _dnsReportedPeerIPAssigned;
	String _rdnsReportedPeerHostName;
	String _srvReportedHostName; // SRV or directly assigned if available.
	String _srvReportedServiceName; // Same as above.
	String _txtReportedKeys;
	bool _hasTXT;

	// Pre SRV.
	String _peerReportedFullHostName;
	String _peerReportedHostName;
	String _peerReportedServiceName;

	void ProcessSRVResult(int index);
	void ProcessAResult(int index);
	void ProcessRDNSResult(int index);
	void ProcessTXTResult(int index);
};

#endif
