#ifndef _MESSAGE_STORAGE_HPP
#define _MESSAGE_STORAGE_HPP

#include "StorageBase.hpp"

class MessageStorage : public Storage
{
public:
	MessageStorage(const uint8_t *ownerKey);
	~MessageStorage();

	void GetFreeTimestampIndex(
		const uint8_t *peerKey,
		int64_t &timestamp,
		int32_t &index);

	bool MessageExists(
		const uint8_t *peerKey,
		int64_t timestamp,
		int32_t index,
		bool incoming);

	void AddMessage(CowBuffer<uint8_t> message);

	CowBuffer<uint8_t> *GetMessageRange(
		int64_t from,
		int64_t to,
		uint64_t &messageCount);

	CowBuffer<uint8_t> *GetMessageRange(
		const uint8_t *peerKey,
		int64_t from,
		int64_t to,
		uint64_t &messageCount);

private:
	const uint8_t *_ownerKey;
};

#endif
