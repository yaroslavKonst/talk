#include "BufferedFile.hpp"

BufferedFile::BufferedFile(int fd)
{
	_fd = fd;
	_bufferSize = 1024;
	_bufferPosition = 0;
	_bufferLimit = 0;
	_buffer = new char[_bufferSize];
}

BufferedFile::~BufferedFile()
{
	delete[] _buffer;
}

bool BufferedFile::Get(char &c)
{
	if (_bufferPosition < _bufferLimit) {
		c = _buffer[_bufferPosition];
		++_bufferPosition;
		return true;
	}

	_bufferPosition = 0;
	_bufferLimit = read(fd, _buffer, _bufferSize);

	if (_bufferLimit <= 0) {
		return false;
	}

	return Get(c);
}
