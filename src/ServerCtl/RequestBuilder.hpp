#ifndef _REQUEST_BUILDER_HPP
#define _REQUEST_BUILDER_HPP

#include "../Common/CowBuffer.hpp"

void PrintHelp();
void PrintShortHelp();

CowBuffer<uint8_t> CreateRequest(int argc, char **argv);

#endif
