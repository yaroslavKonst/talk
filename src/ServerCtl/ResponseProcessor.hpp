#ifndef _RESPONSE_PROCESSOR_HPP
#define _RESPONSE_PROCESSOR_HPP

#include "../Common/CowBuffer.hpp"

int ProcessResponse(int32_t commandId, CowBuffer<uint8_t> response);

#endif
