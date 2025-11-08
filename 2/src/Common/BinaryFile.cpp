#include "BinaryFile.hpp"

#include <unistd.h>
#include <fcntl.h>

BinaryFile::BinaryFile(String path, bool create)
{
	if (create) {
		_fd = open(path.CStr(), O_RDWR | O_CREAT, 0600);
	} else {
		_fd = open(path.CStr(), O_RDWR);
	}

	if (_fd == -1) {
		THROW(String("Failed to open file ") + path + ".");
	}
}

BinaryFile::~BinaryFile()
{
	if (_fd != -1) {
		close(_fd);
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

void BinaryFile::Seek(uint64_t offset)
{
	int64_t res = lseek(_fd, offset, SEEK_SET);

	if (res == -1) {
		THROW("Failed to seek file.");
	}
}
