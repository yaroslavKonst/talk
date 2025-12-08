#include <unistd.h>
#include <cstdlib>
#include <cstdio>

#include "../src/Common/Tree.hpp"
#include "../src/Common/MyString.hpp"

String GetCommand()
{
	String res;
	char c;

	while (read(0, &c, 1) == 1 && c != '\n') {
		res += c;
	}

	return res;
}

int GetDepth(Tree<int>::Entry *entry)
{
	if (!entry) {
		return 0;
	}

	int l = GetDepth(entry->Left);
	int r = GetDepth(entry->Right);

	return l > r ? l + 1 : r + 1;
}

void FillLayer(
	Tree<int>::Entry *entry,
	int layer,
	int *array,
	int *depth,
	int *index)
{
	if (layer) {
		if (entry) {
			FillLayer(entry->Left, layer - 1, array, depth, index);
			FillLayer(entry->Right, layer - 1, array, depth, index);
		} else {
			FillLayer(nullptr, layer - 1, array, depth, index);
			FillLayer(nullptr, layer - 1, array, depth, index);
		}

		return;
	}

	if (!entry) {
		array[*index] = 0;
		depth[*index] = -1;
	} else {
		array[*index] = entry->Key;
		depth[*index] = entry->Depth;
	}

	*index += 1;
}

void PrintLayer(
	Tree<int>::Entry *entry,
	int layer,
	int depth)
{
	int itemCount = 1;

	for (int i = 0; i < layer; i++) {
		itemCount *= 2;
	}

	CowBuffer<int> vals(itemCount);
	CowBuffer<int> depths(itemCount);
	int index = 0;
	FillLayer(entry, layer, vals.Pointer(), depths.Pointer(), &index);

	int offset = 3;
	int startOffset = 0;

	for (int i = depth; i > layer; i--) {
		startOffset += offset / 2 + 1;
		offset = offset + 2 + offset / 2 * 2;
	}

	for (int i = 0; i < startOffset; i++) {
		write(1, " ", 1);
	}

	for (int val = 0; val < (int)vals.Size(); val++) {
		int cp;

		if (vals[val]) {
			cp = printf("%d", vals[val]);
		} else {
			cp = printf(" ");
		}

		fflush(stdout);

		int locOffset = offset + 1 - cp;

		for (int i = 0; i < locOffset; i++) {
			write(1, " ", 1);
		}
	}

	write(1, "\n", 1);

	for (int i = 0; i < startOffset; i++) {
		write(1, " ", 1);
	}

	for (int val = 0; val < (int)depths.Size(); val++) {
		int cp;

		if (depths[val] >= 0) {
			cp = printf("%d", depths[val]);
		} else {
			cp = printf(" ");
		}

		fflush(stdout);

		int locOffset = offset + 1 - cp;

		for (int i = 0; i < locOffset; i++) {
			write(1, " ", 1);
		}
	}

	write(1, "\n", 1);
}

void DumpTree(Tree<int> &tree)
{
	Tree<int>::Entry *root = tree.FindSmallest();

	if (!root) {
		return;
	}

	while (root->Parent) {
		root = root->Parent;
	}

	int depth = GetDepth(root);

	for (int i = 0; i < depth; i++) {
		PrintLayer(root, i, depth);
	}
}

void ListTree(Tree<int> &tree)
{
	Tree<int>::Entry *current = tree.FindSmallest();

	while (current) {
		printf(" %d", current->Key);
		current = tree.Next(current);
	}

	printf("\n");
}

void RListTree(Tree<int> &tree)
{
	Tree<int>::Entry *current = tree.FindBiggest();

	while (current) {
		printf(" %d", current->Key);
		current = tree.Previous(current);
	}

	printf("\n");
}

int main(int argc, char **argv)
{
	Tree<int> tree;

	String command;

	while ((command = GetCommand()).Length()) {
		if (command == "exit") {
			break;
		}

		CowBuffer<String> args = command.Split(' ', true);

		if (!args.Size()) {
			continue;
		}

		if (args[0] == "add") {
			if (args.Size() != 2) {
				printf("No argument given.\n");
				continue;
			}

			bool res = tree.AddEntry(atoi(args[1].CStr()));

			if (res) {
				printf("Added.\n");
			} else {
				printf("Duplicate.\n");
			}


		} else if (args[0] == "del") {
			if (args.Size() != 2) {
				printf("No argument given.\n");
				continue;
			}

			Tree<int>::Entry *entry =
				tree.FindEntry(atoi(args[1].CStr()));

			if (!entry) {
				printf("Not found.\n");
			} else {
				tree.RemoveEntry(entry);
				printf("Removed.\n");
			}
		} else if (args[0] == "dump") {
			DumpTree(tree);
		} else if (args[0] == "list") {
			ListTree(tree);
		} else if (args[0] == "rlist") {
			RListTree(tree);
		} else {
			printf("Unknown command.\n");
		}
	}

	return 0;
}
