#ifndef _TREE_HPP
#define _TREE_HPP

#include <cstdint>

#include "Exception.hpp"

template <typename KT>
class Tree
{
public:
	struct Entry
	{
		KT Key;

		Entry *Parent;
		Entry *Left;
		Entry *Right;

		int32_t Depth;

		Entry()
		{
			memset(this, 0, sizeof(*this));
		}

		~Entry()
		{
			if (Left) {
				delete Left;
			}

			if (Right) {
				delete Right;
			}
		}
	};

	Tree();
	~Tree();

	Entry *FindEntry(const KT &key);

	bool AddEntry(const KT &key);
	void RemoveEntry(Entry *address);

	Entry *FindSmallest(const KT &key);
	Entry *FindBiggest(const KT &key);

	Entry *FindSmallest();
	Entry *FindBiggest();

	Entry *Next(Entry *address);
	Entry *Previous(Entry *address);

private:
	Entry *_root;

	void RotateLeft(Entry *address);
	void RotateRight(Entry *address);

	void RollUp(Entry *address);
	void Swap(Entry *address1, Entry *address2);
};

template <typename KT>
Tree<KT>::Tree()
{
	_root = nullptr;
}

template <typename KT>
Tree<KT>::~Tree()
{
	if (_root) {
		delete _root;
	}
}

template <typename KT>
typename Tree<KT>::Entry *Tree<KT>::FindEntry(const KT &key)
{
	Entry *current = _root;

	while (current) {
		if (key == current->Key) {
			return current;
		}

		if (key < current->Key) {
			current = current->Left;
		} else {
			current = current->Right;
		}
	}

	return nullptr;
}

template <typename KT>
bool Tree<KT>::AddEntry(const KT &key)
{
	Entry **current = &_root;
	Entry *parent = nullptr;

	while (*current) {
		if (key == (*current)->Key) {
			return false;
		}

		parent = *current;

		if (key < (*current)->Key) {
			current = &(*current)->Left;
		} else {
			current = &(*current)->Right;
		}
	}

	Entry *entry = new Entry;
	entry->Key = key;
	entry->Parent = parent;
	entry->Depth = 0;

	*current = entry;

	RollUp(*current);

	return true;
}

template <typename KT>
void Tree<KT>::RemoveEntry(Entry *address)
{
	if (!address->Left) {
		Entry *parent = address->Parent;

		if (parent) {
			if (parent->Left == address) {
				parent->Left = address->Right;
			} else if (parent->Right == address) {
				parent->Right = address->Right;
			} else {
				THROW("Parent entry does not contain "
					"subtree address.");
			}
		} else {
			_root = address->Right;
		}

		if (address->Right) {
			address->Right->Parent = address->Parent;
		}

		address->Left = nullptr;
		address->Right = nullptr;
		delete address;
		RollUp(parent);
	} else if (!address->Right) {
		Entry *parent = address->Parent;

		if (parent) {
			if (parent->Left == address) {
				parent->Left = address->Left;
			} else if (parent->Right == address) {
				parent->Right = address->Left;
			} else {
				THROW("Parent entry does not contain "
					"subtree address.");
			}
		} else {
			_root = address->Left;
		}

		if (address->Left) {
			address->Left->Parent = address->Parent;
		}

		address->Left = nullptr;
		address->Right = nullptr;
		delete address;
		RollUp(parent);
	} else {
		Entry *replacement;

		if (address->Left->Depth > address->Right->Depth) {
			replacement = address->Left;

			while (replacement->Right) {
				replacement = replacement->Right;
			}
		} else {
			replacement = address->Right;

			while (replacement->Left) {
				replacement = replacement->Left;
			}
		}

		Swap(replacement, address);
		RemoveEntry(address);
	}
}

template <typename KT>
typename Tree<KT>::Entry *Tree<KT>::FindSmallest(const KT &key)
{
	Entry *minAddress = nullptr;
	KT minKey;

	Entry *currentAddress = _root;

	while (currentAddress) {
		if (currentAddress->Key == key) {
			return currentAddress;
		}

		if (currentAddress->Key >= key) {
			if (!minAddress || currentAddress->Key < minKey) {
				minAddress = currentAddress;
				minKey = currentAddress->Key;
			}
		}

		if (currentAddress->Key < key) {
			currentAddress = currentAddress->Right;
		} else {
			currentAddress = currentAddress->Left;
		}
	}

	return minAddress;
}

template <typename KT>
typename Tree<KT>::Entry *Tree<KT>::FindBiggest(const KT &key)
{
	Entry *maxAddress = nullptr;
	KT maxKey;

	Entry *currentAddress = _root;

	while (currentAddress) {
		if (currentAddress->Key == key) {
			return currentAddress;
		}

		if (currentAddress->Key <= key) {
			if (!maxAddress || currentAddress->Key > maxKey) {
				maxAddress = currentAddress;
				maxKey = currentAddress->Key;
			}
		}

		if (currentAddress->Key > key) {
			currentAddress = currentAddress->Left;
		} else {
			currentAddress = currentAddress->Right;
		}
	}

	return maxAddress;
}

template <typename KT>
typename Tree<KT>::Entry *Tree<KT>::FindSmallest()
{
	Entry *currentAddress = _root;

	while (currentAddress) {
		if (!currentAddress->Left) {
			return currentAddress;
		}

		currentAddress = currentAddress->Left;
	}

	return nullptr;
}

template <typename KT>
typename Tree<KT>::Entry *Tree<KT>::FindBiggest()
{
	Entry *currentAddress = _root;

	while (currentAddress) {
		if (!currentAddress->Right) {
			return currentAddress;
		}

		currentAddress = currentAddress->Right;
	}

	return nullptr;
}

template <typename KT>
typename Tree<KT>::Entry *Tree<KT>::Next(Entry *address)
{
	if (address->Right) {
		address = address->Right;

		while (address->Left) {
			address = address->Left;
		}

		return address;
	}

	for (;;) {
		Entry *childAddress = address;

		if (!address->Parent) {
			return nullptr;
		}

		address = address->Parent;

		if (childAddress == address->Left) {
			return address;
		}
	}
}

template <typename KT>
typename Tree<KT>::Entry *Tree<KT>::Previous(Entry *address)
{
	if (address->Left) {
		address = address->Left;

		while (address->Right) {
			address = address->Right;
		}

		return address;
	}

	for (;;) {
		Entry *childAddress = address;

		if (!address->Parent) {
			return nullptr;
		}

		address = address->Parent;

		if (childAddress == address->Right) {
			return address;
		}
	}
}

template <typename KT>
void Tree<KT>::RotateLeft(Entry *address)
{
	/* Input argument is the address of node 1.
	 *
	 *      1              2
	 *     / \            / \
	 *    A   2   --->   1   C
	 *       / \        / \
	 *      B   C      A   B
	 *
	 *      1              3
	 *     A \            / \
	 *        2   --->   1   2
	 *       / D        A B C D
	 *      3
	 *     B C
	 */

	Entry *address1 = address;

	if (!address1->Right) {
		return;
	}

	Entry *address2 = address1->Right;

	Entry *addressA = address1->Left;
	Entry *addressB = address2->Left;
	Entry *addressC = address2->Right;

	int32_t depthA = -1;
	int32_t depthB = -1;
	int32_t depthC = -1;

	if (addressA) {
		depthA = addressA->Depth;
	}

	if (addressB) {
		depthB = addressB->Depth;
	}

	if (addressC) {
		depthC = addressC->Depth;
	}

	if (depthB > depthC) {
		RotateRight(address2);
		RotateLeft(address1);
		return;
	}

	Entry *parentAddress = address1->Parent;

	address1->Left = addressA;
	address1->Right = addressB;
	address1->Parent = address2;
	address1->Depth = (depthA > depthB ? depthA : depthB) + 1;

	address2->Left = address1;
	address2->Right = addressC;
	address2->Parent = parentAddress;
	address2->Depth =
		(address1->Depth > depthC ? address1->Depth : depthC) + 1;

	if (parentAddress) {
		if (parentAddress->Left == address1) {
			parentAddress->Left = address2;
		} else if (parentAddress->Right == address1) {
			parentAddress->Right = address2;
		} else {
			THROW("Parent does not have child address.");
		}
	} else {
		_root = address2;
	}

	if (addressB) {
		addressB->Parent = address1;
	}
}

template <typename KT>
void Tree<KT>::RotateRight(Entry *address)
{
	/* Input argument is the address of node 1.
	 *
	 *        1            2
	 *       / \          / \
	 *      2   C  --->  A   1
	 *     / \              / \
	 *    A   B            B   C
	 *
	 *        1            3
	 *       / \          / \
	 *      2   D  --->  2   1
	 *     A \          A B C D
	 *        3
	 *       B C
	 */

	Entry *address1 = address;

	if (!address1->Left) {
		return;
	}

	Entry *address2 = address1->Left;

	Entry *addressA = address2->Left;
	Entry *addressB = address2->Right;
	Entry *addressC = address1->Right;

	int32_t depthA = -1;
	int32_t depthB = -1;
	int32_t depthC = -1;

	if (addressA) {
		depthA = addressA->Depth;
	}

	if (addressB) {
		depthB = addressB->Depth;
	}

	if (addressC) {
		depthC = addressC->Depth;
	}

	if (depthB > depthA) {
		RotateLeft(address2);
		RotateRight(address1);
		return;
	}

	Entry *parentAddress = address1->Parent;

	address1->Left = addressB;
	address1->Right = addressC;
	address1->Parent = address2;
	address1->Depth = (depthB > depthC ? depthB : depthC) + 1;

	address2->Left = addressA;
	address2->Right = address1;
	address2->Parent = parentAddress;
	address2->Depth =
		(address1->Depth > depthA ? address1->Depth : depthA) + 1;

	if (parentAddress) {
		if (parentAddress->Left == address1) {
			parentAddress->Left = address2;
		} else if (parentAddress->Right == address1) {
			parentAddress->Right = address2;
		} else {
			THROW("Parent does not have child address.");
		}
	} else {
		_root = address2;
	}

	if (addressB) {
		addressB->Parent = address1;
	}
}

template <typename KT>
void Tree<KT>::RollUp(Entry *address)
{
	while (address) {
		int32_t leftDepth = 0;
		int32_t rightDepth = 0;

		if (address->Left) {
			leftDepth = address->Left->Depth + 1;
		}

		if (address->Right) {
			rightDepth = address->Right->Depth + 1;
		}

		address->Depth =
			leftDepth > rightDepth ? leftDepth : rightDepth;

		if (rightDepth > leftDepth + 1) {
			RotateLeft(address);
		} else if (leftDepth > rightDepth + 1) {
			RotateRight(address);
		} else {
			address = address->Parent;
		}
	}
}

template <typename KT>
void Tree<KT>::Swap(Entry *address1, Entry *address2)
{
	/*       P1      P2
	 *       |       |
	 *       1       2
	 *      / \     / \
	 *     A1 B1   A2 B2
	 *           |
	 *           V
	 *       P2      P1
	 *       |       |
	 *       1       2
	 *      / \     / \
	 *     A2 B2   A1 B1
	 *
	 * If nodes are directly connected, they will form two loops
	 * that have to be corrected.
	 *
	 *       2       P2
	 *       |       |
	 *       1       2
	 *      / \     / \
	 *     A1 B1   A2  1
	 *           |
	 *           V
	 *       P2      2
	 *       |       |
	 *       1       2
	 *      / \     / \
	 *     A2  1   A1 B1
	 *           |
	 *           V
	 *       P2      1
	 *       |       |
	 *       1       2
	 *      / \     / \
	 *     A2  2   A1 B1
	 */

	if (!address1 || !address2) {
		return;
	}

	Entry *P1 = address1->Parent;
	Entry *P2 = address2->Parent;

	Entry *A1 = address1->Left;
	Entry *A2 = address2->Left;

	Entry *B1 = address1->Right;
	Entry *B2 = address2->Right;

	if (A1) {
		A1->Parent = address2;
	}

	if (A2) {
		A2->Parent = address1;
	}

	if (B1) {
		B1->Parent = address2;
	}

	if (B2) {
		B2->Parent = address1;
	}

	Entry **P1Ref;
	Entry **P2Ref;

	if (P1) {
		if (address1 == P1->Left) {
			P1Ref = &P1->Left;
		} else if (address1 == P1->Right) {
			P1Ref = &P1->Right;
		} else {
			THROW("Parent does not have child address.");
		}
	} else {
		P1Ref = &_root;
	}

	if (P2) {
		if (address2 == P2->Left) {
			P2Ref = &P2->Left;
		} else if (address2 == P2->Right) {
			P2Ref = &P2->Right;
		} else {
			THROW("Parent does not have child address.");
		}
	} else {
		P2Ref = &_root;
	}

	*P1Ref = address2;
	*P2Ref = address1;

	address1->Parent = P2;
	address1->Left = A2;
	address1->Right = B2;

	address2->Parent = P1;
	address2->Left = A1;
	address2->Right = B1;

	// Loop correction.
	if (address1->Parent == address1) {
		address1->Parent = address2;

		if (address2->Left == address2) {
			address2->Left = address1;
		} else if (address2->Right == address2) {
			address2->Right = address1;
		} else {
			THROW("Loop correction failure.");
		}
	}

	if (address2->Parent == address2) {
		address2->Parent = address1;

		if (address1->Left == address1) {
			address1->Left = address2;
		} else if (address1->Right == address1) {
			address1->Right = address2;
		} else {
			THROW("Loop correction failure.");
		}
	}
}

#endif
