#ifndef _BINARY_FILE_HPP
#define _BINARY_FILE_HPP

#include <cstdint>
#include <unistd.h>

#include "MyString.hpp"
#include "Exception.hpp"

class BinaryFile
{
public:
	BinaryFile(String path, bool create);
	~BinaryFile();

	uint64_t Size();

	template <typename T>
	void Read(
		T *buffer,
		uint64_t elementCount,
		uint64_t offsetInBytes)
	{
		Seek(offsetInBytes);

		int64_t res = read(_fd, buffer, sizeof(T) * elementCount);

		if (res != (int64_t)(sizeof(T) * elementCount)) {
			THROW("Failed to read data from file.");
		}
	}

	template <typename T>
	void Write(
		const T *buffer,
		uint64_t elementCount,
		uint64_t offsetInBytes)
	{
		Seek(offsetInBytes);

		int64_t res = write(_fd, buffer, sizeof(T) * elementCount);

		if (res != (int64_t)(sizeof(T) * elementCount)) {
			THROW("Failed to write data to file.");
		}
	}

	void Clear();

private:
	int _fd;

	void Seek(uint64_t offset);
};

#endif
