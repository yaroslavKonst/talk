#ifndef _MESSAGE_STORAGE_HPP
#define _MESSAGE_STORAGE_HPP

#include "StorageBase.hpp"
#include "../Common/BinaryFile.hpp"

class MessageStorageIndex
{
public:
	MessageStorageIndex(String path);

	bool EntryExists(int64_t timestamp, int32_t index, bool incoming);

	void AddEntry(int64_t timestamp, int32_t index, bool incoming);
	void RemoveEntry(int64_t timestamp, int32_t index, bool incoming);

	void GetEntry(
		uint32_t address,
		int64_t &timestamp,
		int32_t &index,
		bool &incoming);

	uint32_t FindSmallest(int64_t timestamp);
	uint32_t FindBiggest();

	uint32_t Next(uint32_t address);
	uint32_t Previous(uint32_t address);

private:
	BinaryFile _file;

	// Binary search tree.
	struct EntryValue
	{
		int64_t Timestamp;
		int32_t Index;
		uint8_t Incoming;

		bool operator==(const EntryValue &entry) const
		{
			return
				Timestamp == entry.Timestamp &&
				Index == entry.Index &&
				Incoming == entry.Incoming;
		}

		bool operator<(const EntryValue &entry) const
		{
			if (Timestamp != entry.Timestamp) {
				return Timestamp < entry.Timestamp;
			}

			if (Index != entry.Index) {
				return Index < entry.Index;
			}

			return Incoming < entry.Incoming;
		}
	};

	struct IndexEntry
	{
		EntryValue Value;

		int8_t Valid;

		uint32_t This;
		uint32_t Left;
		uint32_t Right;
		uint32_t Parent;

		// Distance from the most distant leaf to this node.
		// Zero for leaves.
		uint32_t Depth;
	};

	class TreeCache;

	class CacheEntry
	{
		friend TreeCache;
	public:
		operator IndexEntry()
		{
			return _entry;
		}

		void operator=(const IndexEntry &entry)
		{
			_entry = entry;
			_file->Write<IndexEntry>(
				&_entry,
				1,
				sizeof(IndexEntry) * _entry.This);
		}

	private:
		IndexEntry _entry;
		BinaryFile *_file;

		CacheEntry *_left;
		CacheEntry *_right;

		CacheEntry(BinaryFile *file, uint32_t address)
		{
			_file = file;
			_left = nullptr;
			_right = nullptr;

			if (_file->Size() > sizeof(IndexEntry) * address) {
				_file->Read<IndexEntry>(
					&_entry,
					1,
					sizeof(IndexEntry) * address);
			} else {
				memset(&_entry, 0, sizeof(_entry));
				_entry.This = address;
				_file->Write<IndexEntry>(
					&_entry,
					1,
					sizeof(IndexEntry) * _entry.This);
			}

			_entry.This = address;
		}

		~CacheEntry()
		{
			if (_left) {
				delete _left;
			}

			if (_right) {
				delete _right;
			}
		}
	};

	class TreeCache
	{
	public:
		TreeCache(BinaryFile *file)
		{
			_file = file;
			_root = nullptr;
		}

		~TreeCache()
		{
			if (_root) {
				delete _root;
			}
		}

		CacheEntry &operator[](uint32_t address)
		{
			CacheEntry **node = &_root;

			while (*node) {
				if (address == (*node)->_entry.This) {
					return **node;
				}

				if (address < (*node)->_entry.This) {
					node = &((*node)->_left);
				} else {
					node = &((*node)->_right);
				}
			}

			*node = new CacheEntry(_file, address);
			return **node;
		}

	private:
		BinaryFile *_file;
		CacheEntry *_root;
	};

	TreeCache _cache;

	// File starts with special node. Its right subtree is the address
	// of the root node. Its left subtree is the address of the first
	// empty node.
	//
	// Zero is used as null address.
	//
	// Used nodes have all fields filled with data.
	// Empty nodes have address of the next empty node in the
	// address in the right subtree.
	// Invalid nodes are still in the tree structure but their
	// values are considered not stored in the tree.

	// Rotate tree from right to left.
	void RotateLeft(uint32_t address);
	// Go from address to root, recalculate depth, rotate tree if
	// needed. Free nodes that can be removed.
	void Rollup(uint32_t address);

	uint32_t Allocate();
	void Free(uint32_t address);
};

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

	CowBuffer<CowBuffer<uint8_t>> GetMessageRange(int64_t from, int64_t to);

	CowBuffer<CowBuffer<uint8_t>> GetMessageRange(
		const uint8_t *peerKey,
		int64_t from,
		int64_t to);

	CowBuffer<CowBuffer<uint8_t>> GetLatestNMessages(
		const uint8_t *peerKey,
		int requestedMessageCount);

private:
	const uint8_t *_ownerKey;
};

#endif
