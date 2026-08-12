#ifndef _NETWORKING_HPP
#define _NETWORKING_HPP

#include <sys/socket.h>
#include <netinet/in.h>

#include "MyString.hpp"

struct IPAddress
{
	enum class AddressType
	{
		IPv4,
		IPv6
	};

	AddressType Type;

	union AddressStorage
	{
		uint32_t IPv4;
		uint8_t IPv6[16];
	};

	AddressStorage Address;

	IPAddress();

	bool operator==(const IPAddress &addr) const;
	bool operator!=(const IPAddress &addr) const;
	bool operator<(const IPAddress &addr) const;

	struct sockaddr_storage *GetStructSockaddr(
		uint16_t port,
		int &addrLen) const;
	bool LoadStructSockaddr(struct sockaddr_storage *addr);
	bool ParseIPAddress(String addressString);
	String ToString() const;
};

#endif
