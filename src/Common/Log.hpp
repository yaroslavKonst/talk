#ifndef _LOG_HPP
#define _LOG_HPP

#include "MyString.hpp"

enum class LogLevel
{
	Debug = 0,
	Verbose = 1,
	Info = 2,
	Warning = 3,
	Error = 4,
	Fatal = 5
};

void Log(LogLevel level, String section, String message);
void SetMaxLogWidth(int width);
void SetLogLevel(LogLevel level);

#endif
