#include "StorageBase.hpp"

#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "../Common/Exception.hpp"

void Storage::CreateDirectory(String path)
{
	int fd = open(path.CStr(), O_RDONLY | O_DIRECTORY);

	if (fd == -1) {
		if (errno != ENOENT) {
			THROW("Error on checking directory.");
		}

		fd = mkdir(path.CStr(), 0700);

		if (fd == -1) {
			THROW("Failed to create directory.");
		}
	} else {
		close(fd);
	}
}

bool Storage::FileExists(String path)
{
	int fd = open(path.CStr(), O_RDONLY);

	if (fd == -1) {
		return false;
	}

	close(fd);
	return true;
}

CowBuffer<String> Storage::ListDirectory(String path)
{
	DIR *dir = opendir(path.CStr());

	if (!dir) {
		THROW("Failed to open directory.");
	}

	struct Entry
	{
		Entry *Next;
		String Name;
	};

	Entry *first = nullptr;
	Entry **last = &first;

	struct dirent *dent;
	int entryCount = 0;

	while ((dent = readdir(dir)) != nullptr) {
		String name = dent->d_name;

		if (name == "." || name == "..") {
			continue;
		}

		++entryCount;

		*last = new Entry;
		(*last)->Next = nullptr;
		(*last)->Name = dent->d_name;
		last = &((*last)->Next);
	}

	if (!entryCount) {
		return CowBuffer<String>();
	}

	CowBuffer<String> result(entryCount);

	for (int i = 0; i < entryCount; i++) {
		result[i] = first->Name;

		Entry *tmp = first;
		first = first->Next;
		delete tmp;
	}

	closedir(dir);

	return result;
}
