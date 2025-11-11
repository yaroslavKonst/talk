#ifndef _MESSAGE_CONTAINER_HPP
#define _MESSAGE_CONTAINER_HPP

#include <sys/types.h>
#include <dirent.h>

#include "CowBuffer.hpp"
#include "MyString.hpp"

class MessageContainer
{
public:
	MessageContainer(const uint8_t *key1, const uint8_t *key2);
	~MessageContainer();

	void AddMessage(CowBuffer<uint8_t> message);

	CowBuffer<uint8_t> *GetMessageRange(
		int64_t from,
		int64_t to,
		uint64_t &messageCount);

private:
	String _root;

	DIR *_dir;

	void CreateDirectory(String path);
	bool FileExists(String path);

	struct Elem
	{
		Elem *Next;
		String Name;
	};
};

#endif
