#ifndef _STORAGE_BASE_HPP
#define _STORAGE_BASE_HPP

#include "../Common/MyString.hpp"
#include "../Common/CowBuffer.hpp"

class Storage
{
protected:
	void CreateDirectory(String path);
	bool FileExists(String path);

	CowBuffer<String> ListDirectory(String path);
};

#endif
