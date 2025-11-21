#ifndef _FILE_HPP
#define _FILE_HPP

#include "../Common/MyString.hpp"
#include "../Common/CowBuffer.hpp"

void CreateDirectory(String path);
bool FileExists(String path);
CowBuffer<String> ListDirectory(String path);

#endif
