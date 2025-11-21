#ifndef _ATTRIBUTE_STORAGE_HPP
#define _ATTRIBUTE_STORAGE_HPP

#include "../Common/CowBuffer.hpp"

#define ATTRIBUTE_READ 0x1
#define ATTRIBUTE_SENT 0x2
#define ATTRIBUTE_FAILURE 0x4

class AttributeStorage
{
public:
	AttributeStorage(const uint8_t *ownerKey);
	~AttributeStorage();

	void SetAttribute(CowBuffer<uint8_t> message, uint32_t attribute);
	uint32_t GetAttribute(CowBuffer<uint8_t> message);

private:
	const uint8_t *_ownerKey;
};

#endif
