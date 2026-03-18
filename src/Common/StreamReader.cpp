#include "StreamReader.hpp"

#include <unistd.h>
#include <errno.h>

StreamReader::StreamReader(int fd, unsigned long size)
{
	_fd = fd;
	_buffer = CowBuffer<uint8_t>(size);
	_readBytes = 0;
}

bool StreamReader::Read()
{
	if (_readBytes >= _buffer.Size()) {
		return true;
	}

	for (;;) {
		long bs = read(
			_fd,
			_buffer.Pointer(_readBytes),
			_buffer.Size() - _readBytes);

		if (bs == -1) {
			if (errno == EINTR) {
				continue;
			}

			return false;
		}

		if (bs == 0) {
			return false;
		}

		_readBytes += bs;

		return true;
	}
}

bool StreamReader::ReadingEnd()
{
	return _readBytes >= _buffer.Size();
}

CowBuffer<uint8_t> StreamReader::GetBuffer()
{
	return _buffer;
}
