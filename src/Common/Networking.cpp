#include "Networking.hpp"

#include <string.h>
#include <arpa/inet.h>

#include "Exception.hpp"

IPAddress::IPAddress()
{
	memset(this, 0, sizeof(*this));
}

bool IPAddress::operator==(const IPAddress &addr) const
{
	if (Type != addr.Type) {
		return false;
	}

	if (Type == AddressType::IPv4) {
		return Address.IPv4 == addr.Address.IPv4;
	}

	return !memcmp(Address.IPv6, addr.Address.IPv6, 16);
}

bool IPAddress::operator!=(const IPAddress &addr) const
{
	return !(*this == addr);
}

bool IPAddress::operator<(const IPAddress &addr) const
{
	if (Type != addr.Type) {
		return Type < addr.Type;
	}

	if (Type == AddressType::IPv4) {
		return Address.IPv4 < addr.Address.IPv4;
	}

	int res = memcmp(Address.IPv6, addr.Address.IPv6, 16);
	return res < 0;
}

struct sockaddr_storage *IPAddress::GetStructSockaddr(
	uint16_t port,
	int &addrLen) const
{
	struct sockaddr_storage *addrBase = new struct sockaddr_storage;
	memset(addrBase, 0, sizeof(struct sockaddr_storage));

	if (Type == AddressType::IPv4) {
		struct sockaddr_in *addr = (struct sockaddr_in*)addrBase;
		addr->sin_family = AF_INET;
		addr->sin_port = port;
		addr->sin_addr.s_addr = Address.IPv4;

		addrLen = sizeof(struct sockaddr_in);
	} else if (Type == AddressType::IPv6) {
		struct sockaddr_in6 *addr = (struct sockaddr_in6*)addrBase;
		addr->sin6_family = AF_INET6;
		addr->sin6_port = port;
		memcpy(addr->sin6_addr.s6_addr, Address.IPv6, 16);

		addrLen = sizeof(struct sockaddr_in6);
	} else {
		delete addrBase;
		THROW("Unsupported address family.");
	}

	return addrBase;
}

bool IPAddress::LoadStructSockaddr(struct sockaddr_storage *addr)
{
	if (addr->ss_family == AF_INET) {
		struct sockaddr_in *addrIn = (struct sockaddr_in*)addr;
		Type = AddressType::IPv4;
		Address.IPv4 = addrIn->sin_addr.s_addr;
		return true;
	}

	if (addr->ss_family == AF_INET6) {
		struct sockaddr_in6 *addrIn = (struct sockaddr_in6*)addr;
		Type = AddressType::IPv6;
		memcpy(Address.IPv6, addrIn->sin6_addr.s6_addr, 16);
		return true;
	}

	return false;
}

bool IPAddress::ParseIPAddress(String addressString)
{
	int res = inet_pton(AF_INET, addressString.CStr(), &Address.IPv4);

	if (res) {
		Type = AddressType::IPv4;
		return true;
	}

	res = inet_pton(AF_INET6, addressString.CStr(), Address.IPv6);

	if (res) {
		Type = AddressType::IPv6;
		return true;
	}

	return false;
}

String IPAddress::ToString() const
{
	char ipStr[INET6_ADDRSTRLEN];

	int family;

	if (Type == AddressType::IPv4) {
		family = AF_INET;
	} else if (Type == AddressType::IPv6) {
		family = AF_INET6;
	} else {
		THROW("Invalid address family in conversion.");
	}

	if (!inet_ntop(family, &Address, ipStr, INET6_ADDRSTRLEN)) {
		return "";
	}

	return ipStr;
}
