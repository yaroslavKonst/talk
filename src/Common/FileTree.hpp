#ifndef _FILE_TREE_HPP
#define _FILE_TREE_HPP

#include "BinaryFile.hpp"

template <typename KT, typename VT>
class FileTree
{
public:
	FileTree(const String path);

	uint32_t FindEntry(const KT &key);

	bool AddEntry(const KT &key, const VT &value);
	void RemoveEntry(uint32_t address);

	VT GetEntry(uint32_t address);
	void SetEntry(uint32_t address, const VT &value);

	uint32_t FindSmallest(const KT &key);
	uint32_t FindBiggest(const KT &key);

	uint32_t FindSmallest();
	uint32_t FindBiggest();

	uint32_t Next(uint32_t address);
	uint32_t Previous(uint32_t address);

private:
	BinaryFile _file;

	struct Entry
	{
		KT Key;
		VT Value;

		uint32_t This;
		uint32_t Parent;
		uint32_t Left;
		uint32_t Right;

		int32_t Depth;
	};

	class Cache;

	class CacheEntry
	{
		friend Cache;
	public:
		operator Entry()
		{
			return _entry;
		}

		void operator=(const Entry &entry)
		{
			_entry = entry;
			_file->Write<Entry>(
				&_entry,
				1,
				sizeof(Entry) * _entry.This);
		}

	private:
		Entry _entry;
		BinaryFile *_file;

		CacheEntry *_left;
		CacheEntry *_right;

		CacheEntry(BinaryFile *file, uint32_t address)
		{
			_file = file;
			_left = nullptr;
			_right = nullptr;

			if (_file->Size() > sizeof(Entry) * address) {
				_file->Read<Entry>(
					&_entry,
					1,
					sizeof(Entry) * address);
				_entry.This = address;
			} else {
				memset(&_entry, 0, sizeof(_entry));
				_entry.This = address;
				_file->Write<Entry>(
					&_entry,
					1,
					sizeof(Entry) * _entry.This);
			}
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

	class Cache
	{
	public:
		Cache(BinaryFile *file)
		{
			_file = file;
			_root = nullptr;
		}

		~Cache()
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
					node = &(*node)->_left;
				} else {
					node = &(*node)->_right;
				}
			}

			*node = new CacheEntry(_file, address);
			return **node;
		}

	private:
		BinaryFile *_file;
		CacheEntry *_root;
	};

	Cache _cache;

	void RotateLeft(uint32_t address);
	void RotateRight(uint32_t address);

	void RollUp(uint32_t address);
	void Swap(uint32_t address1, uint32_t address2);

	uint32_t Allocate();
	void Free(uint32_t address);
};

template <typename KT, typename VT>
FileTree<KT, VT>::FileTree(const String path) :
	_file(path, true),
	_cache(&_file)
{
	if (_file.Size() == 0) {
		Entry entry;
		memset(&entry, 0, sizeof(entry));
		_file.Write<Entry>(&entry, 1, 0);
	}
}

template <typename KT, typename VT>
uint32_t FileTree<KT, VT>::FindEntry(const KT &key)
{
	Entry md = _cache[0];

	uint32_t currentAddress = md.Right;

	while (currentAddress) {
		Entry entry = _cache[currentAddress];

		if (key == entry.Key) {
			return currentAddress;
		}

		if (key < entry.Key) {
			currentAddress = entry.Left;
		} else {
			currentAddress = entry.Right;
		}
	}

	return 0;
}

template <typename KT, typename VT>
bool FileTree<KT, VT>::AddEntry(const KT &key, const VT &value)
{
	Entry parentEntry = _cache[0];

	uint32_t *currentAddress = &parentEntry.Right;

	while (*currentAddress) {
		Entry entry = _cache[*currentAddress];

		if (key == entry.Key) {
			return false;
		}

		if (key < entry.Key) {
			currentAddress = &parentEntry.Left;
		} else {
			currentAddress = &parentEntry.Right;
		}

		parentEntry = entry;
	}

	uint32_t newAddress = Allocate();

	Entry entry;
	memset(&entry, 0, sizeof(entry));

	entry.This = newAddress;
	entry.Key = key;
	entry.Value = value;
	entry.Parent = parentEntry.This;
	entry.Depth = 0;

	_cache[entry.This] = entry;

	parentEntry = _cache[parentEntry.This];
	*currentAddress = newAddress;
	_cache[parentEntry.This] = parentEntry;

	RollUp(newAddress);

	return true;
}

template <typename KT, typename VT>
void FileTree<KT, VT>::RemoveEntry(uint32_t address)
{
	Entry entry = _cache[address];

	if (!entry.Left) {
		Entry parentEntry = _cache[entry.Parent];

		if (parentEntry.Left == entry.This) {
			parentEntry.Left = entry.Right;
		} else if (parentEntry.Right == entry.This) {
			parentEntry.Right = entry.Right;
		} else {
			THROW("Parent entry does not contain subtree address.");
		}

		_cache[parentEntry.This] = parentEntry;

		if (entry.Right) {
			Entry rightSubtree = _cache[entry.Right];
			rightSubtree.Parent = entry.Parent;
			_cache[entry.Right] = rightSubtree;
		}

		Free(entry.This);
		RollUp(entry.Parent);
	} else if (!entry.Right) {
		Entry parentEntry = _cache[entry.Parent];

		if (parentEntry.Left == entry.This) {
			parentEntry.Left = entry.Left;
		} else if (parentEntry.Right == entry.This) {
			parentEntry.Right = entry.Left;
		} else {
			THROW("Parent entry does not contain subtree address.");
		}

		_cache[parentEntry.This] = parentEntry;

		if (entry.Left) {
			Entry leftSubtree = _cache[entry.Left];
			leftSubtree.Parent = entry.Parent;
			_cache[entry.Left] = leftSubtree;
		}

		Free(entry.This);
		RollUp(entry.Parent);
	} else {
		Entry replacement;
		int32_t leftDepth = 0;
		int32_t rightDepth = 0;

		replacement = _cache[entry.Left];
		leftDepth = replacement.Depth + 1;

		replacement = _cache[entry.Right];
		rightDepth = replacement.Depth + 1;

		if (leftDepth > rightDepth) {
			replacement = _cache[entry.Left];

			while (replacement.Right) {
				replacement = _cache[replacement.Right];
			}
		} else {
			replacement = _cache[entry.Right];

			while (replacement.Left) {
				replacement = _cache[replacement.Left];
			}
		}

		Swap(replacement.This, entry.This);
		RemoveEntry(entry.This);
	}
}

template <typename KT, typename VT>
VT FileTree<KT, VT>::GetEntry(uint32_t address)
{
	Entry entry = _cache[address];
	return entry.Value;
}

template <typename KT, typename VT>
void FileTree<KT, VT>::SetEntry(uint32_t address, const VT &value)
{
	Entry entry = _cache[address];
	entry.Value = value;
	_cache[address] = entry;
}

template <typename KT, typename VT>
uint32_t FileTree<KT, VT>::FindSmallest(const KT &key)
{
	Entry md = _cache[0];

	uint32_t minAddress = 0;
	KT minKey;

	uint32_t currentAddress = md.Right;

	while (currentAddress) {
		Entry entry = _cache[currentAddress];

		if (entry.Key == key) {
			return currentAddress;
		}

		if (entry.Key >= key) {
			if (!minAddress || entry.Key < minKey) {
				minAddress = currentAddress;
				minKey = entry.Key;
			}
		}

		if (entry.Key < key) {
			currentAddress = entry.Right;
		} else {
			currentAddress = entry.Left;
		}
	}

	return minAddress;
}

template <typename KT, typename VT>
uint32_t FileTree<KT, VT>::FindBiggest(const KT &key)
{
	Entry md = _cache[0];

	uint32_t maxAddress = 0;
	KT maxKey;

	uint32_t currentAddress = md.Right;

	while (currentAddress) {
		Entry entry = _cache[currentAddress];

		if (entry.Key == key) {
			return currentAddress;
		}

		if (entry.Key <= key) {
			if (!maxAddress || entry.Key > maxKey) {
				maxAddress = currentAddress;
				maxKey = entry.Key;
			}
		}

		if (entry.Key > key) {
			currentAddress = entry.Left;
		} else {
			currentAddress = entry.Right;
		}
	}

	return maxAddress;
}

template <typename KT, typename VT>
uint32_t FileTree<KT, VT>::FindSmallest()
{
	Entry md = _cache[0];
	uint32_t currentAddress = md.Right;

	while (currentAddress) {
		Entry entry = _cache[currentAddress];

		if (!entry.Left) {
			return entry.This;
		}

		currentAddress = entry.Left;
	}

	return 0;
}

template <typename KT, typename VT>
uint32_t FileTree<KT, VT>::FindBiggest()
{
	Entry md = _cache[0];
	uint32_t currentAddress = md.Right;

	while (currentAddress) {
		Entry entry = _cache[currentAddress];

		if (!entry.Right) {
			return entry.This;
		}

		currentAddress = entry.Right;
	}

	return 0;
}

template <typename KT, typename VT>
uint32_t FileTree<KT, VT>::Next(uint32_t address)
{
	Entry entry = _cache[address];

	if (entry.Right) {
		entry = _cache[entry.Right];

		while (entry.Left) {
			entry = _cache[entry.Left];
		}

		return entry.This;
	}

	for (;;) {
		uint32_t childAddress = entry.This;

		if (!entry.Parent) {
			return 0;
		}

		entry = _cache[entry.Parent];

		if (childAddress == entry.Left) {
			return entry.This;
		}
	}
}

template <typename KT, typename VT>
uint32_t FileTree<KT, VT>::Previous(uint32_t address)
{
	Entry entry = _cache[address];

	if (entry.Left) {
		entry = _cache[entry.Left];

		while (entry.Right) {
			entry = _cache[entry.Right];
		}

		return entry.This;
	}

	for (;;) {
		uint32_t childAddress = entry.This;

		if (!entry.Parent) {
			return 0;
		}

		entry = _cache[entry.Parent];

		if (childAddress == entry.Right) {
			return entry.This;
		}
	}
}

template <typename KT, typename VT>
void FileTree<KT, VT>::RotateLeft(uint32_t address)
{
	uint32_t address1 = address;
	Entry node1 = _cache[address1];

	if (!node1.Right) {
		return;
	}

	uint32_t address2 = node1.Right;
	Entry node2 = _cache[address2];

	uint32_t addressA = node1.Left;
	uint32_t addressB = node2.Left;
	uint32_t addressC = node2.Right;

	int32_t depthA = -1;
	int32_t depthB = -1;
	int32_t depthC = -1;

	if (addressA) {
		depthA = _cache[addressA].operator Entry().Depth;
	}

	if (addressB) {
		depthB = _cache[addressB].operator Entry().Depth;
	}

	if (addressC) {
		depthC = _cache[addressC].operator Entry().Depth;
	}

	if (depthB > depthC) {
		RotateRight(address2);
		RotateLeft(address1);
		return;
	}

	uint32_t parentAddress = node1.Parent;
	Entry parentNode = _cache[parentAddress];

	node1.Left = addressA;
	node1.Right = addressB;
	node1.Parent = address2;
	node1.Depth = (depthA > depthB ? depthA : depthB) + 1;
	_cache[address1] = node1;

	node2.Left = address1;
	node2.Right = addressC;
	node2.Parent = parentAddress;
	node2.Depth = (node1.Depth > depthC ? node1.Depth : depthC) + 1;
	_cache[address2] = node2;

	if (parentNode.Left == address1) {
		parentNode.Left = address2;
	} else if (parentNode.Right == address1) {
		parentNode.Right = address2;
	} else {
		THROW("Parent does not have child address.");
	}

	_cache[parentAddress] = parentNode;

	if (addressB) {
		Entry nodeB = _cache[addressB];
		nodeB.Parent = address1;
		_cache[addressB] = nodeB;
	}
}

template <typename KT, typename VT>
void FileTree<KT, VT>::RotateRight(uint32_t address)
{
	uint32_t address1 = address;
	Entry node1 = _cache[address1];

	if (!node1.Left) {
		return;
	}

	uint32_t address2 = node1.Left;
	Entry node2 = _cache[address2];

	uint32_t addressA = node2.Left;
	uint32_t addressB = node2.Right;
	uint32_t addressC = node1.Right;

	int32_t depthA = -1;
	int32_t depthB = -1;
	int32_t depthC = -1;

	if (addressA) {
		depthA = _cache[addressA].operator Entry().Depth;
	}

	if (addressB) {
		depthB = _cache[addressB].operator Entry().Depth;
	}

	if (addressC) {
		depthC = _cache[addressC].operator Entry().Depth;
	}

	if (depthB > depthA) {
		RotateLeft(address2);
		RotateRight(address1);
		return;
	}

	uint32_t parentAddress = node1.Parent;
	Entry parentNode = _cache[parentAddress];

	node1.Left = addressB;
	node1.Right = addressC;
	node1.Parent = address2;
	node1.Depth = (depthB > depthC ? depthB : depthC) + 1;
	_cache[address1] = node1;

	node2.Left = addressA;
	node2.Right = address1;
	node2.Parent = parentAddress;
	node2.Depth = (node1.Depth > depthA ? node1.Depth : depthA) + 1;
	_cache[address2] = node2;

	if (parentNode.Left == address1) {
		parentNode.Left = address2;
	} else if (parentNode.Right == address1) {
		parentNode.Right = address2;
	} else {
		THROW("Parent does not have child address.");
	}

	_cache[parentAddress] = parentNode;

	if (addressB) {
		Entry nodeB = _cache[addressB];
		nodeB.Parent = address1;
		_cache[addressB] = nodeB;
	}
}

template <typename KT, typename VT>
void FileTree<KT, VT>::RollUp(uint32_t address)
{
	while (address) {
		Entry entry = _cache[address];

		uint32_t leftDepth = 0;
		uint32_t rightDepth = 0;

		if (entry.Left) {
			Entry left = _cache[entry.Left];
			leftDepth = left.Depth + 1;
		}

		if (entry.Right) {
			Entry right = _cache[entry.Right];
			rightDepth = right.Depth + 1;
		}

		entry.Depth = leftDepth > rightDepth ? leftDepth : rightDepth;
		_cache[entry.This] = entry;

		if (rightDepth > leftDepth + 1) {
			RotateLeft(address);
		} else if (leftDepth > rightDepth + 1) {
			RotateRight(address);
		} else {
			address = entry.Parent;
		}
	}
}

template <typename KT, typename VT>
void FileTree<KT, VT>::Swap(uint32_t address1, uint32_t address2)
{
	if (!address1 || !address2) {
		return;
	}

	Entry node1 = _cache[address1];
	Entry node2 = _cache[address2];

	uint32_t P1 = node1.Parent;
	uint32_t P2 = node2.Parent;

	uint32_t A1 = node1.Left;
	uint32_t A2 = node2.Left;

	uint32_t B1 = node1.Right;
	uint32_t B2 = node2.Right;

	if (A1) {
		Entry nodeA1 = _cache[A1];
		nodeA1.Parent = address2;
		_cache[A1] = nodeA1;
	}

	if (A2) {
		Entry nodeA2 = _cache[A2];
		nodeA2.Parent = address1;
		_cache[A2] = nodeA2;
	}

	if (B1) {
		Entry nodeB1 = _cache[B1];
		nodeB1.Parent = address2;
		_cache[B1] = nodeB1;
	}

	if (B2) {
		Entry nodeB2 = _cache[B2];
		nodeB2.Parent = address1;
		_cache[B2] = nodeB2;
	}

	Entry nodeP1 = _cache[P1];
	Entry nodeP2 = _cache[P2];

	if (address1 == nodeP1.Left) {
		nodeP1.Left = address2;
	} else if (address1 == nodeP1.Right) {
		nodeP1.Right = address2;
	} else {
		THROW("Parent does not have child address.");
	}

	if (address2 == nodeP2.Left) {
		nodeP2.Left = address1;
	} else if (address2 == nodeP2.Right) {
		nodeP2.Right = address1;
	} else {
		THROW("Parent does not have child address.");
	}

	_cache[P1] = nodeP1;
	_cache[P2] = nodeP2;

	node1 = _cache[address1];
	node2 = _cache[address2];

	node1.Parent = P2;
	node1.Left = A2;
	node1.Right = B2;

	node2.Parent = P1;
	node2.Left = A1;
	node2.Right = B1;

	if (node1.Parent == address1) {
		node1.Parent = address2;

		if (node2.Left == address2) {
			node2.Left = address1;
		} else if (node2.Right == address2) {
			node2.Right = address1;
		} else {
			THROW("Loop correction failure.");
		}
	}

	if (node2.Parent == address2) {
		node2.Parent = address1;

		if (node1.Left == address1) {
			node1.Left = address2;
		} else if (node1.Right == address1) {
			node1.Right = address2;
		} else {
			THROW("Loop correction failure.");
		}
	}

	_cache[address1] = node1;
	_cache[address2] = node2;
}

template <typename KT, typename VT>
uint32_t FileTree<KT, VT>::Allocate()
{
	Entry md = _cache[0];

	if (!md.Left) {
		return _file.Size() / sizeof(Entry);
	}

	uint32_t newAddress = md.Left;

	Entry allocatedEntry = _cache[newAddress];

	md.Left = allocatedEntry.Right;
	_cache[0] = md;
	return newAddress;
}

template <typename KT, typename VT>
void FileTree<KT, VT>::Free(uint32_t address)
{
	Entry md = _cache[0];
	Entry entry = _cache[address];

	entry.Right = md.Left;
	md.Left = address;

	_cache[address] = entry;
	_cache[0] = md;
}

#endif
