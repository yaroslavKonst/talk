#ifndef _MESSAGE_STORAGE_HPP
#define _MESSAGE_STORAGE_HPP

#include "StorageBase.hpp"
#include "../Common/BinaryFile.hpp"

class MessageStorage : public Storage
{
public:
	MessageStorage(const uint8_t *ownerKey);
	~MessageStorage();

	void GetFreeTimestampIndex(
		const uint8_t *peerKey,
		int64_t timestamp,
		int32_t &index);

	bool MessageExists(
		const uint8_t *peerKey,
		int64_t timestamp,
		int32_t index,
		bool incoming);

	bool AddMessage(CowBuffer<uint8_t> message);

	CowBuffer<CowBuffer<uint8_t>> GetMessageRange(
		int64_t from,
		int64_t to,
		uint64_t &messageCount);

	CowBuffer<CowBuffer<uint8_t>> GetMessageRange(
		const uint8_t *peerKey,
		int64_t from,
		int64_t to,
		uint64_t &messageCount);

	CowBuffer<CowBuffer<uint8_t>> GetLatestNMessages(
		const uint8_t *peerKey,
		uint64_t requestedMessageCount,
		uint64_t &messageCount);

private:
	const uint8_t *_ownerKey;

	// Binary search tree.
	struct IndexEntry
	{
		int64_t Timestamp;
		int32_t Index;

		uint64_t Left;
		uint64_t Right;
		uint64_t Parent;

		bool operator<(const IndexEntry &entry) const;
	};

	String GetNameForMessage(CowBuffer<uint8_t> message);
};

#endif
