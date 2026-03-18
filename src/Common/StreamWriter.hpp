#ifndef _STREAM_WRITER_HPP
#define _STREAM_WRITER_HPP

#include "CowBuffer.hpp"

class StreamWriter
{
public:
	StreamWriter(int fd, CowBuffer<uint8_t> buffer);

	// True: write successful.
	// False: write ended with error.
	bool Write();

	// True: all required data was written.
	// False: there is still data to be written.
	bool WritingEnd();

private:
	int _fd;
	unsigned long _writtenBytes;
	CowBuffer<uint8_t> _buffer;
};

#endif
