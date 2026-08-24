#include "StreamWriter.hpp"

#include <unistd.h>
#include <errno.h>

StreamWriter::StreamWriter(int fd, const CowBuffer<uint8_t> buffer) :
	_buffer(buffer)
{
	_fd = fd;
	_writtenBytes = 0;
}

bool StreamWriter::Write()
{
	if (_writtenBytes >= _buffer.Size()) {
		return true;
	}

	for (;;) {
		int bs = write(
			_fd,
			_buffer.Pointer(_writtenBytes),
			_buffer.Size() - _writtenBytes);

		if (bs == -1) {
			if (errno == EINTR) {
				continue;
			}

			return false;
		}

		if (bs == 0) {
			return false;
		}

		_writtenBytes += bs;

		return true;
	}
}

bool StreamWriter::WritingEnd()
{
	return _writtenBytes >= _buffer.Size();
}
