#ifndef _STREAM_READER_HPP
#define _STREAM_READER_HPP

#include "CowBuffer.hpp"

class StreamReader
{
public:
	StreamReader(int fd, unsigned long size);

	// True: read successful.
	// False: read ended with error.
	bool Read();

	// True: all required data was read.
	// False: there is still data to be read.
	bool ReadingEnd();

	CowBuffer<uint8_t> GetBuffer();

private:
	int _fd;
	unsigned long _readBytes;
	CowBuffer<uint8_t> _buffer;
};

#endif
