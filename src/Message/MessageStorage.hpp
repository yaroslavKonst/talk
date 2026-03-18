#ifndef _MESSAGE_STORAGE_HPP
#define _MESSAGE_STORAGE_HPP

#include <cstring>

#include "Message.hpp"
#include "../Common/FileTree.hpp"
#include "../Common/CowBuffer.hpp"

class MessageStorage
{
public:
	MessageStorage(const uint8_t *ownerKey);
	~MessageStorage();

	Message::MessageID GetFreeHeader(
		const uint8_t *peerKey,
		int64_t timestamp);

	bool MessageExists(const Message::MessageID &id);

	bool AddMessage(CowBuffer<uint8_t> message);
	CowBuffer<uint8_t> GetMessage(const Message::MessageID &id);

	CowBuffer<Message::MessageID> GetMessageRange(
		int64_t from,
		int64_t to);

	CowBuffer<Message::MessageID> GetMessageRange(
		const uint8_t *peerKey,
		int64_t from,
		int64_t to);

private:
	const uint8_t *_ownerKey;
	String _rootPath;
};

#endif
