#ifndef _LOG_HPP
#define _LOG_HPP

#include <ctime>
#include <cstdio>

#include "MyString.hpp"
#include "UnixTime.hpp"

inline void Log(String message)
{
	int64_t timestamp = GetUnixTime();
	String timeStr = ctime(&timestamp);
	timeStr = timeStr.Substring(0, timeStr.Length() - 1);

	printf("[%s]: %s\n", timeStr.CStr(), message.CStr());
}

#endif
