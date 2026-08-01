#include "CommandLineParser.hpp"

#include "Exception.hpp"

CommandLineParser::CommandLineParser()
{ }

CommandLineParser::CommandLineParser(
	const CowBuffer<String> keys,
	const CowBuffer<String> multiKeys,
	const CowBuffer<String> args)
{
	_keyNames = keys;
	_keyValues = CowBuffer<bool>(keys.Size());

	for (unsigned int i = 0; i < keys.Size(); i++) {
		_keyValues[i] = false;
	}

	_multiKeyNames = multiKeys;
	_multiKeyValues = CowBuffer<int>(multiKeys.Size());

	for (unsigned int i = 0; i < multiKeys.Size(); i++) {
		_multiKeyValues[i] = 0;
	}

	_argumentNames = args;
	_argumentValues = CowBuffer<String>(_argumentNames.Size());
}

bool CommandLineParser::Parse(int argc, char **argv)
{
	struct PosArg
	{
		PosArg *Next;
		String Text;
	};

	PosArg *firstPosArg = nullptr;
	PosArg *lastPosArg = nullptr;
	int posArgCount = 0;

	int expectedArgIndex = -1;
	bool doKeyProcessing = true;

	bool failure = false;

	for (int i = 0; i < argc; i++) {
		String word = argv[i];

		if (expectedArgIndex >= 0) {
			_argumentValues[expectedArgIndex] = word;
			expectedArgIndex = -1;
			continue;
		}

		if (doKeyProcessing && word == "--") {
			doKeyProcessing = false;
			continue;
		}

		bool isKey =
			i > 0 &&
			doKeyProcessing &&
			word.Length() > 1 &&
			word.CStr()[0] == '-';

		if (isKey) {
			int index = FindKey(word);

			if (index != -1) {
				_keyValues[index] = true;
				continue;
			}

			index = FindMultiKey(word);

			if (index != -1) {
				_multiKeyValues[index] += 1;
				continue;
			}

			index = FindArg(word);

			if (index != -1) {
				expectedArgIndex = index;
				continue;
			}

			_errorDescription = "Unknown key: " + word + ".";
			failure = true;
			break;
		}

		PosArg *arg = new PosArg;
		arg->Next = nullptr;
		arg->Text = word;

		if (!firstPosArg) {
			firstPosArg = arg;
			lastPosArg = arg;
		} else {
			lastPosArg->Next = arg;
			lastPosArg = arg;
		}

		++posArgCount;
	}

	if (expectedArgIndex != -1) {
		_errorDescription =
			"Missing value for " +
			_argumentNames[expectedArgIndex] + ".";
		failure = true;
	}

	if (failure) {
		while (firstPosArg) {
			PosArg *tmp = firstPosArg;
			firstPosArg = firstPosArg->Next;
			delete tmp;
		}

		lastPosArg = nullptr;
		return false;
	}

	_positionalArguments = CowBuffer<String>(posArgCount);
	int index = 0;

	while (firstPosArg) {
		_positionalArguments[index] = firstPosArg->Text;
		++index;

		PosArg *tmp = firstPosArg;
		firstPosArg = firstPosArg->Next;
		delete tmp;
	}

	return true;
}

String CommandLineParser::GetErrorString()
{
	return _errorDescription;
}

const CowBuffer<String> &CommandLineParser::GetPositionalArguments()
{
	return _positionalArguments;
}

bool CommandLineParser::GetKeyValue(String key)
{
	int index = FindKey(key);

	if (index == -1) {
		THROW("Unknown key " + key + ".");
	}

	return _keyValues[index];
}

int CommandLineParser::GetMultiKeyValue(String multiKey)
{
	int index = FindMultiKey(multiKey);

	if (index == -1) {
		THROW("Unknown multikey " + multiKey + ".");
	}

	return _multiKeyValues[index];
}

String CommandLineParser::GetArgumentValue(String arg)
{
	int index = FindArg(arg);

	if (index == -1) {
		THROW("Unknown argument " + arg + ".");
	}

	return _argumentValues[index];
}

int CommandLineParser::FindKey(String word)
{
	for (unsigned int i = 0; i < _keyNames.Size(); i++) {
		if (word == _keyNames[i]) {
			return i;
		}
	}

	return -1;
}

int CommandLineParser::FindMultiKey(String word)
{
	for (unsigned int i = 0; i < _multiKeyNames.Size(); i++) {
		if (word == _multiKeyNames[i]) {
			return i;
		}
	}

	return -1;
}

int CommandLineParser::FindArg(String word)
{
	for (unsigned int i = 0; i < _argumentNames.Size(); i++) {
		if (word == _argumentNames[i]) {
			return i;
		}
	}

	return -1;
}
