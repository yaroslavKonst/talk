#include "BinaryFile.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "FileAccess.hpp"

BinaryFile::BinaryFile(String path, bool create, bool trunc)
{
	int flags = O_RDWR;

	if (create) {
		flags |= O_CREAT;
	}

	if (trunc) {
		flags |= O_TRUNC;
	}

	_fd = open(path.CStr(), flags, FileAccessConstants::FileAccessRights);

	if (_fd == -1) {
		THROW("Failed to open file " + path + ".");
	}
}

BinaryFile::~BinaryFile()
{
	if (_fd != -1) {
		bool intr;

		do {
			intr = false;
			int res = close(_fd);

			if (res == -1 && errno == EINTR) {
				intr = true;
			}
		} while (intr);

		_fd = -1;
	}
}

uint64_t BinaryFile::Size()
{
	int64_t size = lseek(_fd, 0, SEEK_END);

	if (size == -1) {
		THROW("Failed to seek file.");
	}

	return size;
}

void BinaryFile::Clear()
{
	bool intr;

	do {
		intr = false;
		int res = ftruncate(_fd, 0);

		if (res == -1) {
			if (errno == EINTR) {
				intr = true;
			} else {
				THROW("Failed to clear file.");
			}
		}
	} while (intr);

	Seek(0);
}

void BinaryFile::Seek(uint64_t offset)
{
	int64_t res = lseek(_fd, offset, SEEK_SET);

	if (res == -1) {
		THROW("Failed to seek file.");
	}
}
