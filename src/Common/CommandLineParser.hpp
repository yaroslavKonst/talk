#ifndef _COMMAND_LINE_PARSER_HPP
#define _COMMAND_LINE_PARSER_HPP

#include "MyString.hpp"
#include "CowBuffer.hpp"

// Command line argsuments starting with '-' are considered to be
// keys (one word, its presence represents a boolean value),
// multikeys (same as keys, but can be present multiple times and are counted)
// or argument names (the next word after argument name is treated as
// argument value).
//
// All other words are considered to be positional arguments.
// If some positional argument is '--' then all subsequent arguments
// are considered to be positional, key parsing stops.
class CommandLineParser
{
public:
	CommandLineParser();

	CommandLineParser(
		const CowBuffer<String> keys,
		const CowBuffer<String> multiKeys,
		const CowBuffer<String> args);

	bool Parse(int argc, char **argv);
	String GetErrorString();

	const CowBuffer<String> &GetPositionalArguments();

	bool GetKeyValue(String key);
	int GetMultiKeyValue(String multiKey);
	String GetArgumentValue(String arg);

private:
	CowBuffer<String> _keyNames;
	CowBuffer<bool> _keyValues;

	CowBuffer<String> _multiKeyNames;
	CowBuffer<int> _multiKeyValues;

	CowBuffer<String> _argumentNames;
	CowBuffer<String> _argumentValues;

	CowBuffer<String> _positionalArguments;

	String _errorDescription;

	int FindKey(String word);
	int FindMultiKey(String word);
	int FindArg(String word);
};

#endif
