#include "GateSecurityModule.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../Message/Message.hpp"
#include "../Common/Hex.hpp"

GateSecurityModule::GateSecurityModule(EventDispatcher *dispatcher) :
	_resolver(dispatcher)
{
	_failure = false;
	_user = nullptr;

	_ipOnlyHostName = false;

	_knownPeerIPAssigned = false;
	_dnsReportedPeerIPAssigned = false;
	_hasTXT = false;

	_resolver.SetResolverUser(this);
}

GateSecurityModule::~GateSecurityModule()
{
	_resolver.SetResolverUser(nullptr);
	_resolver.PassOwnership();
	_requests.Resize(0);
}

void GateSecurityModule::SetUser(ResolverUser *user)
{
	_user = user;
}

bool GateSecurityModule::Failure()
{
	return _failure;
}

void GateSecurityModule::ClearFailure()
{
	_failure = false;
}

void GateSecurityModule::ResolveCompleted()
{
	for (uint32_t i = 0; i < _requests.Size(); i++) {
		Resolver::RequestBase::Type t = _requests[i]->T;

		if (t == Resolver::RequestBase::Type::SRV) {
			ProcessSRVResult(i);
		} else if (t == Resolver::RequestBase::Type::A) {
			ProcessAResult(i);
		} else if (t == Resolver::RequestBase::Type::AAAA) {
			ProcessAAAAResult(i);
		} else if (t == Resolver::RequestBase::Type::RDNS) {
			ProcessRDNSResult(i);
		} else if (t == Resolver::RequestBase::Type::TXT) {
			ProcessTXTResult(i);
		}

		delete _requests[i];
	}

	_requests.Resize(0);

	if (_user) {
		_user->ResolveCompleted();
	}
}

void GateSecurityModule::SetKnownPeerIP(IPAddress ip)
{
	_knownPeerIP = ip;
	_knownPeerIPAssigned = true;

	if (_dnsReportedPeerIPAssigned && _knownPeerIP != _dnsReportedPeerIP) {
		_failure = true;
		return;
	}

	if (_ipOnlyHostName && _srvReportedHostName.Length()) {
		IPAddress address;

		if (!address.ParseIPAddress(_srvReportedHostName)) {
			_failure = true;
			return;
		}

		if (address != _knownPeerIP) {
			_failure = true;
		}
	}
}

void GateSecurityModule::SetKnownPeerFullHostName(String fullHostName)
{
	if (!Message::VerifyFullHostName(fullHostName)) {
		_failure = true;
		return;
	}

	_knownPeerFullHostName = fullHostName;

	if (_peerReportedFullHostName.Length()) {
		if (_peerReportedFullHostName != _knownPeerFullHostName) {
			_failure = true;
			return;
		}
	}

	bool res = Message::SplitFullHostName(
		fullHostName,
		_knownPeerHostName,
		_knownPeerServiceName);

	if (!res) {
		_failure = true;
		return;
	}

	if (_knownPeerHostName.Length() && _knownPeerServiceName.Length()) {
		_srvReportedHostName = _knownPeerHostName;
		_srvReportedServiceName = _knownPeerServiceName;
	}

	IPAddress testAddr;

	if (testAddr.ParseIPAddress(_knownPeerHostName)) {
		_ipOnlyHostName = true;
	}

	if (_ipOnlyHostName && !_knownPeerServiceName.Length()) {
		_failure = true;
		return;
	}

	if (_ipOnlyHostName && _knownPeerIPAssigned) {
		IPAddress address;

		if (!address.ParseIPAddress(_srvReportedHostName)) {
			_failure = true;
			return;
		}

		if (address != _knownPeerIP) {
			_failure = true;
		}
	}
}

void GateSecurityModule::SetPeerReportedFullHostName(String fullHostName)
{
	if (!Message::VerifyFullHostName(fullHostName)) {
		_failure = true;
		return;
	}

	_peerReportedFullHostName = fullHostName;

	if (_knownPeerFullHostName.Length()) {
		if (_peerReportedFullHostName != _knownPeerFullHostName) {
			_failure = true;
			return;
		}
	}

	bool res = Message::SplitFullHostName(
		fullHostName,
		_peerReportedHostName,
		_peerReportedServiceName);

	if (!res) {
		_failure = true;
		return;
	}

	if (_peerReportedHostName.Length() && _peerReportedServiceName.Length())
	{
		_srvReportedHostName = _peerReportedHostName;
		_srvReportedServiceName = _peerReportedServiceName;
	}

	IPAddress testAddr;

	if (testAddr.ParseIPAddress(_peerReportedHostName)) {
		_ipOnlyHostName = true;
	}

	if (_ipOnlyHostName && !_peerReportedServiceName.Length()) {
		_failure = true;
		return;
	}

	if (_ipOnlyHostName && _knownPeerIPAssigned) {
		IPAddress address;

		if (!address.ParseIPAddress(_srvReportedHostName)) {
			_failure = true;
			return;
		}

		if (address != _knownPeerIP) {
			_failure = true;
		}
	}
}

String GateSecurityModule::GetFullHostName()
{
	if (_knownPeerFullHostName.Length()) {
		return _knownPeerFullHostName;
	}

	if (_peerReportedFullHostName.Length()) {
		return _peerReportedFullHostName;
	}

	THROW("Peer host name is unknown.");
}

String GateSecurityModule::GetSRVReportedServiceName()
{
	if (!_srvReportedServiceName.Length()) {
		THROW("Service name is unknown.");
	}

	return _srvReportedServiceName;
}

IPAddress GateSecurityModule::GetDNSReportedIP()
{
	if (!_dnsReportedPeerIPAssigned) {
		THROW("DNS address is unknown.");
	}

	return _dnsReportedPeerIP;
}

bool GateSecurityModule::IsIPOnlyHost()
{
	return _ipOnlyHostName;
}

void GateSecurityModule::AcceptHostNameAsIP()
{
	if (!_ipOnlyHostName) {
		THROW("Forbidden in DNS mode.");
	}

	IPAddress address;

	bool res = address.ParseIPAddress(_srvReportedHostName);

	if (!res) {
		_failure = true;
		return;
	}

	_dnsReportedPeerIP = address;
	_dnsReportedPeerIPAssigned = true;
}

bool GateSecurityModule::NeedSRV()
{
	if (_ipOnlyHostName) {
		return false;
	}

	return !_srvReportedHostName.Length() ||
		!_srvReportedServiceName.Length();
}

void GateSecurityModule::RunSRV()
{
	if (!_peerReportedHostName.Length() && !_knownPeerHostName.Length()) {
		THROW("No input for SRV request.");
	}

	String knownName;

	if (_knownPeerHostName.Length()) {
		knownName = _knownPeerHostName;
	} else {
		knownName = _peerReportedHostName;
	}

	Resolver::RequestSRV *srv = new Resolver::RequestSRV;
	srv->DNSName = knownName;
	srv->ServiceName = "talkdgate";
	srv->TCP = true;

	_requests.Resize(1);
	_requests[0] = srv;

	_resolver.StartAsyncResolve(_requests);
}

bool GateSecurityModule::NeedA()
{
	if (_ipOnlyHostName) {
		return false;
	}

	if (_knownPeerIPAssigned) {
		if (_knownPeerIP.Type == IPAddress::AddressType::IPv6) {
			return false;
		}
	}

	return !_dnsReportedPeerIPAssigned;
}

void GateSecurityModule::RunA()
{
	if (!_srvReportedHostName.Length()) {
		THROW("No input for A request.");
	}

	Resolver::RequestA *a = new Resolver::RequestA;
	a->DNSName = _srvReportedHostName;

	_requests.Resize(1);
	_requests[0] = a;

	_resolver.StartAsyncResolve(_requests);
}

bool GateSecurityModule::NeedAAAA()
{
	if (_ipOnlyHostName) {
		return false;
	}

	if (_knownPeerIPAssigned) {
		if (_knownPeerIP.Type == IPAddress::AddressType::IPv4) {
			return false;
		}
	}

	return !_dnsReportedPeerIPAssigned;
}

void GateSecurityModule::RunAAAA()
{
	if (!_srvReportedHostName.Length()) {
		THROW("No input for AAAA request.");
	}

	Resolver::RequestAAAA *aaaa = new Resolver::RequestAAAA;
	aaaa->DNSName = _srvReportedHostName;

	_requests.Resize(1);
	_requests[0] = aaaa;

	_resolver.StartAsyncResolve(_requests);
}

bool GateSecurityModule::NeedParams()
{
	if (_ipOnlyHostName) {
		return false;
	}

	return !_rdnsReportedPeerHostName.Length() ||
		!_hasTXT;
}

void GateSecurityModule::RunParams()
{
	if (!_knownPeerIPAssigned && !_dnsReportedPeerIPAssigned) {
		THROW("No input for rDNS request.");
	}

	if (!_srvReportedHostName.Length()) {
		THROW("No input for TXT request.");
	}

	Resolver::RequestRDNS *rdns = new Resolver::RequestRDNS;
	Resolver::RequestTXT *txt = new Resolver::RequestTXT;

	rdns->IP = _knownPeerIPAssigned ? _knownPeerIP : _dnsReportedPeerIP;
	txt->DNSName = _srvReportedHostName;

	_requests.Resize(2);
	_requests[0] = rdns;
	_requests[1] = txt;

	_resolver.StartAsyncResolve(_requests);
}

void GateSecurityModule::ValidateSyn(
	const CowBuffer<uint8_t> buffer,
	const CowBuffer<uint8_t> signature)
{
	if (_ipOnlyHostName && signature.Size()) {
		_failure = true;
		return;
	}

	if ((bool)_txtReportedKeys.Length() != (bool)signature.Size()) {
		_failure = true;
		return;
	}

	if (!_txtReportedKeys.Length()) {
		return;
	}

	CowBuffer<String> signatureKeys = _txtReportedKeys.Split(';', true);

	for (uint32_t keyIdx = 0; keyIdx < signatureKeys.Size(); keyIdx++) {
		Crypto::X25519::SignaturePublicKeyContainer sPubKey;

		String keyHex = signatureKeys[keyIdx];

		bool validLength =
			keyHex.Length() ==
			Crypto::X25519::SIGNATURE_PUBLIC_KEY_SIZE * 2;

		if (!validLength) {
			continue;
		}

		keyHex = keyHex.ToLowerCase();

		try {
			HexToData(keyHex, sPubKey.Key);
		} catch (Exception &ex) {
			continue;
		}

		bool validSignature = Crypto::X25519::Verify(
			buffer.Slice(0, buffer.Size() - signature.Size()),
			sPubKey,
			signature.Pointer());

		if (!validSignature) {
			continue;
		}

		return;
	}

	_failure = true;
}

void GateSecurityModule::RunFullValidation()
{
	if (!_knownPeerIPAssigned) {
		_failure = true;
		return;
	}

	if (!_srvReportedHostName.Length() ||
		!_srvReportedServiceName.Length())
	{
		_failure = true;
		return;
	}

	if (!Message::VerifyPortName(_srvReportedServiceName)) {
		_failure = true;
		return;
	}

	if (_ipOnlyHostName) {
		IPAddress address;

		if (!address.ParseIPAddress(_srvReportedHostName)) {
			_failure = true;
			return;
		}

		if (address != _knownPeerIP) {
			_failure = true;
		}

		return;
	}

	if (NeedSRV() || NeedA() || NeedAAAA() || NeedParams()) {
		_failure = true;
		return;
	}

	if (_srvReportedHostName != _rdnsReportedPeerHostName) {
		_failure = true;
		return;
	}

	if (!_dnsReportedPeerIPAssigned) {
		_failure = true;
		return;
	}

	if (_knownPeerIP != _dnsReportedPeerIP) {
		_failure = true;
	}
}

void GateSecurityModule::ProcessSRVResult(int index)
{
	Resolver::RequestSRV *srv = static_cast<Resolver::RequestSRV*>(
		_requests[index]);

	if (srv->Status || !srv->Result) {
		_failure = true;
		return;
	}

	_srvReportedHostName = srv->Result->Target;
	_srvReportedServiceName = ToString(ntohs(srv->Result->Port));

	if (!_srvReportedHostName.Length() ||
		!_srvReportedServiceName.Length())
	{
		_failure = true;
	}
}

void GateSecurityModule::ProcessAResult(int index)
{
	Resolver::RequestA *a = static_cast<Resolver::RequestA*>(
		_requests[index]);

	if (a->Status) {
		_failure = true;
		return;
	}

	_dnsReportedPeerIP = a->ResultIP;
	_dnsReportedPeerIPAssigned = true;

	if (_knownPeerIPAssigned) {
		if (_knownPeerIP != _dnsReportedPeerIP) {
			_failure = true;
			return;
		}
	}
}

void GateSecurityModule::ProcessAAAAResult(int index)
{
	Resolver::RequestAAAA *aaaa = static_cast<Resolver::RequestAAAA*>(
		_requests[index]);

	if (aaaa->Status) {
		_failure = true;
		return;
	}

	_dnsReportedPeerIP = aaaa->ResultIP;
	_dnsReportedPeerIPAssigned = true;

	if (_knownPeerIPAssigned) {
		if (_knownPeerIP != _dnsReportedPeerIP) {
			_failure = true;
		}
	}
}

void GateSecurityModule::ProcessRDNSResult(int index)
{
	Resolver::RequestRDNS *rdns = static_cast<Resolver::RequestRDNS*>(
		_requests[index]);

	if (rdns->Status) {
		_failure = true;
		return;
	}

	_rdnsReportedPeerHostName = rdns->ResultName;

	if (!_rdnsReportedPeerHostName.Length()) {
		_failure = true;
		return;
	}

	if (_rdnsReportedPeerHostName != _srvReportedHostName) {
		_failure = true;
	}
}

void GateSecurityModule::ProcessTXTResult(int index)
{
	_hasTXT = true;

	Resolver::RequestTXT *txt = static_cast<Resolver::RequestTXT*>(
		_requests[index]);

	if (txt->Status) {
		return;
	}

	String rawTXT = txt->Result;
	String keySectionName = "talkdkey=";

	int pos = 0;

	for (;;) {
		pos = rawTXT.Find(keySectionName);

		if (pos == -1) {
			return;
		}

		if (pos > 0 && rawTXT.CStr()[pos - 1] != '\n') {
			rawTXT = rawTXT.Substring(
				pos + 1,
				rawTXT.Length() - pos - 1);
			continue;
		}

		break;
	}

	pos += keySectionName.Length();

	String keyLine = rawTXT.Substring(pos, rawTXT.Length() - pos);

	if (!keyLine.Length()) {
		_failure = true;
		return;
	}

	if (keyLine.CStr()[0] != '"') {
		_failure = true;
		return;
	}

	int quotePos = 1;

	while (quotePos < keyLine.Length()) {
		if (keyLine.CStr()[quotePos] == '"') {
			break;
		}

		++quotePos;
	}

	if (quotePos >= keyLine.Length()) {
		_failure = true;
		return;
	}

	_txtReportedKeys = keyLine.Substring(1, quotePos - 1);

	if (!_txtReportedKeys.Length()) {
		_failure = true;
		return;
	}

	if (_ipOnlyHostName && _txtReportedKeys.Length()) {
		_failure = true;
	}
}
