#include <unistd.h>
#include <cstdlib>
#include <cstdio>

#include "../src/Common/FileTree.hpp"
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

void ListTree(FileTree<long, int> &tree)
{
	uint32_t current = tree.FindSmallest();

	while (current) {
		printf(" %d", tree.GetEntry(current));
		current = tree.Next(current);
	}

	printf("\n");
}

void RListTree(FileTree<long, int> &tree)
{
	uint32_t current = tree.FindBiggest();

	while (current) {
		printf(" %d", tree.GetEntry(current));
		current = tree.Previous(current);
	}

	printf("\n");
}

int main(int argc, char **argv)
{
	FileTree<long, int> tree("TestTreeFile.bin");

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

			bool res = tree.AddEntry(
				atoi(args[1].CStr()),
				atoi(args[1].CStr()));

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

			uint32_t entry =
				tree.FindEntry(atoi(args[1].CStr()));

			if (!entry) {
				printf("Not found.\n");
			} else {
				tree.RemoveEntry(entry);
				printf("Removed.\n");
			}
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
