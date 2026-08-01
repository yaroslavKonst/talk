#ifndef _REQUEST_BUILDER_HPP
#define _REQUEST_BUILDER_HPP

#include "../Common/CowBuffer.hpp"
#include "../Common/MyString.hpp"

void PrintHelp();
void PrintShortHelp();

CowBuffer<uint8_t> CreateRequest(const CowBuffer<String> args);

#endif
