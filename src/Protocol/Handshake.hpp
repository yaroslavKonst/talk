#ifndef _HANDSHAKE_HPP
#define _HANDSHAKE_HPP

#include "../Common/CowBuffer.hpp"

namespace Handshake1
{
	struct Data
	{
		const uint8_t *Key;
		int64_t Timestamp;
		CowBuffer<uint8_t> Signature;
	};

	bool Parse(const CowBuffer<uint8_t> buffer, Data &result);
	CowBuffer<uint8_t> Build(const Data &data, const uint8_t *signatureKey);
}

namespace Handshake2
{
	struct Data
	{
		const uint8_t *Key;
		int64_t Timestamp;
	};

	bool Parse(const CowBuffer<uint8_t> buffer, Data &result);
	CowBuffer<uint8_t> Build(const Data &data);
}

namespace Handshake3
{
	struct Data
	{
		int64_t Timestamp;
	};

	bool Parse(const CowBuffer<uint8_t> buffer, Data &result);
	CowBuffer<uint8_t> Build(const Data &data);
}

#endif
