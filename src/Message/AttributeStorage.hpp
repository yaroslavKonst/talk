#ifndef _ATTRIBUTE_STORAGE_HPP
#define _ATTRIBUTE_STORAGE_HPP

#include "Message.hpp"
#include "../Common/CowBuffer.hpp"
#include "../Common/MyString.hpp"

#define ATTRIBUTE_READ 0x1
#define ATTRIBUTE_SENT 0x2
#define ATTRIBUTE_FAILURE 0x4

class AttributeStorage
{
public:
	AttributeStorage(const uint8_t *ownerKey);
	~AttributeStorage();

	void SetAttribute(const Message::MessageID &id, uint32_t attribute);
	uint32_t GetAttribute(const Message::MessageID &id);

	CowBuffer<Message::MessageID> ListUnsent();
	CowBuffer<Message::MessageID> ListUnread();

private:
	const uint8_t *_ownerKey;
	String _rootPath;
};

#endif
