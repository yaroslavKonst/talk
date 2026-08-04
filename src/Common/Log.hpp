#ifndef _LOG_HPP
#define _LOG_HPP

#include <cstdio>

#include "MyString.hpp"
#include "UnixTime.hpp"

void Log(String section, String message);
void AllowMultilineLog(bool allow);

#endif
