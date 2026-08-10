#ifndef _FILE_HPP
#define _FILE_HPP

#include "../Common/MyString.hpp"
#include "../Common/CowBuffer.hpp"

void CreateDirectory(String path);
void RemoveDirectory(String path);
bool FileExists(String path);
CowBuffer<String> ListDirectory(String path);
void MakeNonblocking(int fd);
void DeleteFile(String path);

#endif
