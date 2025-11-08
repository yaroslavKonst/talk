#ifndef _BUFFERED_FILE_HPP
#define _BUFFERED_FILE_HPP

class BufferedFile
{
public:
	BufferedFile(int fd);
	~BufferedFile();

	bool Get(char &c);
	bool End();

private:
	int _fd;
	char *_buffer;
	int _bufferSize;
	int _bufferPosition;
	int _bufferLimit;
};

#endif
