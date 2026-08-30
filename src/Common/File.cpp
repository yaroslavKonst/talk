#include "File.hpp"

#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "Exception.hpp"
#include "FileAccess.hpp"

void CreateDirectory(String path)
{
	int fd = open(path.CStr(), O_RDONLY | O_DIRECTORY);

	if (fd == -1) {
		if (errno != ENOENT) {
			THROW("Error on checking directory.");
		}

		fd = mkdir(
			path.CStr(),
			FileAccessConstants::DirectoryAccessRights);

		if (fd == -1) {
			THROW("Failed to create directory.");
		}
	} else {
		close(fd);
	}
}

void RemoveDirectory(String path)
{
	int res = rmdir(path.CStr());

	if (res == -1) {
		THROW("Failed to remove " + path + " directory.");
	}
}

bool FileExists(String path)
{
	int fd = open(path.CStr(), O_RDONLY);

	if (fd == -1) {
		return false;
	}

	close(fd);
	return true;
}

CowBuffer<String> ListDirectory(String path)
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

void MakeNonblocking(int fd)
{
	int flags = fcntl(fd, F_GETFL);

	if (flags == -1) {
		THROW("Failed to get fd flags.");
	}

	flags |= O_NONBLOCK;

	int res = fcntl(fd, F_SETFL, flags);

	if (res == -1) {
		THROW("Failed to set fd flags.");
	}
}

void DeleteFile(String path)
{
	int res = unlink(path.CStr());

	if (res == -1) {
		THROW("Failed to unlink " + path + ".");
	}
}
