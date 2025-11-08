#ifndef _EXCEPTION_HPP
#define _EXCEPTION_HPP

#include "MyString.hpp"

#define THROW(msg) throw Exception(msg, __FILE__, __LINE__)

class Exception
{
public:
	Exception(String message, String file, int line)
	{
		_message = message;
		_file = file;
		_line = line;
	}

	String Message() const
	{
		return _message;
	}

	String File() const
	{
		return _file;
	}

	int Line() const
	{
		return _line;
	}

	String What() const
	{
		return _file + ": " + ToString(_line) + ": " + _message;
	}

private:
	String _message;
	String _file;
	int _line;
};

#endif
