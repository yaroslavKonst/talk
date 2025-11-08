#include "MyString.hpp"

#include <cstring>

static bool IsSpace(char c)
{
	return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

String::String()
{
	_data = 0;
	Clear();
}

String::String(const String &s)
{
	_data = s._data;
	IncRef();
}

String::String(const char *s)
{
	if (!s) {
		_data = 0;
		Clear();
		return;
	}

	int length = strlen(s);

	if (length == 0) {
		_data = 0;
		Clear();
		return;
	}

	_data = new Data;
	_data->RefCount = 1;
	_data->Length = length;
	_data->Reserved = length + 1;
	_data->Data = new char[length + 1];

	memcpy(_data->Data, s, length);
	_data->Data[length] = 0;
}

String::~String()
{
	FreeRef();
}

void String::Clear()
{
	FreeRef();

	_data = new Data;
	_data->RefCount = 1;
	_data->Length = 0;
	_data->Reserved = 1;
	_data->Data = new char[1];
	_data->Data[0] = 0;
}

String &String::operator=(const String &s)
{
	FreeRef();
	_data = s._data;
	IncRef();

	return *this;
}

String String::operator+(const String &s) const
{
	int l1 = _data->Length;
	int l2 = s._data->Length;

	int length = l1 + l2;

	if (length == 0) {
		return String();
	}

	Data *data = new Data;
	data->RefCount = 0;
	data->Length = length;
	data->Reserved = length + 1;
	data->Data = new char[length + 1];

	if (l1) {
		memcpy(data->Data, _data->Data, l1);
	}

	if (l2) {
		memcpy(data->Data + l1, s._data->Data, l2);
	}

	data->Data[length] = 0;

	return String(data);
}

void String::operator+=(const String &s)
{
	*this = *this + s;
}

void String::operator+=(char c)
{
	MakeExclusive();

	if (_data->Length >= _data->Reserved - 1) {
		char *buffer = _data->Data;
		_data->Reserved = _data->Reserved * 2;
		_data->Data = new char[_data->Reserved];
		memcpy(_data->Data, buffer, _data->Length + 1);
		delete[] buffer;
	}

	_data->Data[_data->Length] = c;
	_data->Length += 1;
	_data->Data[_data->Length] = 0;
}

const char *String::CStr() const
{
	return _data->Data;
}

int String::Length() const
{
	return _data->Length;
}

bool String::operator==(const String &s) const
{
	if (_data->Length != s._data->Length) {
		return false;
	}

	for (int i = 0; i < _data->Length; i++) {
		if (_data->Data[i] != s._data->Data[i]) {
			return false;
		}
	}

	return true;
}

bool String::operator<(const String &s) const
{
	int length =
		_data->Length < s._data->Length ?
		_data->Length :
		s._data->Length;

	for (int i = 0; i < length; i++) {
		if (_data->Data[i] != s._data->Data[i]) {
			return _data->Data[i] < s._data->Data[i];
		}
	}

	return _data->Length < s._data->Length;
}

String *String::Split(char delim, bool removeEmpty, int &partCount) const
{
	if (_data->Length == 0) {
		partCount = 1;
		return new String[1];
	}

	partCount = 1;

	for (int i = 0; i < _data->Length; i++) {
		if (_data->Data[i] == delim) {
			bool newPart = !removeEmpty ||
				(i > 0 && _data->Data[i - 1] != delim);

			if (newPart) {
				++partCount;
			}
		}
	}

	String *result = new String[partCount];

	int startIndex = 0;
	int partIndex = 0;

	for (int currIndex = 0; currIndex < _data->Length; currIndex++) {
		if (_data->Data[currIndex] != delim) {
			continue;
		}

		if (startIndex == currIndex) {
			if (!removeEmpty) {
				result[partIndex] = String();
				++partIndex;
			}
		} else {
			int partLength = currIndex - startIndex;
			char *buffer = new char[partLength + 1];

			memcpy(
				buffer,
				_data->Data + startIndex,
				partLength);
			buffer[partLength] = 0;

			result[partIndex] = String(buffer);
			++partIndex;
			delete[] buffer;
		}

		startIndex = currIndex + 1;
	}

	if (startIndex < _data->Length) {
		result[partIndex] = String(_data->Data + startIndex);
		++partIndex;
	} else if (!removeEmpty) {
		result[partIndex] = String();
	}

	return result;
}

String String::Trim() const
{
	int startIndex = 0;
	int endIndex = _data->Length - 1;

	while (IsSpace(_data->Data[startIndex]) && startIndex < _data->Length) {
		++startIndex;
	}

	if (startIndex >= _data->Length) {
		return String();
	}

	while (IsSpace(_data->Data[endIndex]) && endIndex >= startIndex) {
		--endIndex;
	}

	Data *data = new Data;
	data->RefCount = 0;
	data->Length = endIndex - startIndex + 1;
	data->Reserved = data->Length + 1;
	data->Data = new char[data->Reserved];

	memcpy(data->Data, _data->Data + startIndex, data->Length);
	data->Data[data->Length] = 0;

	return String(data);
}

String String::Substring(int start, int length) const
{
	Data *data = new Data;
	data->RefCount = 0;
	data->Length = length;
	data->Reserved = length + 1;
	data->Data = new char[data->Reserved];

	if (length) {
		memcpy(data->Data, _data->Data + start, length);
	}

	data->Data[length] = 0;

	return String(data);
}

void String::Wipe()
{
	MakeExclusive();

	for (int i = 0; i < _data->Length; i++) {
		_data->Data[i] = 0;
	}

	Clear();
}

String::String(Data *data)
{
	_data = data;
	IncRef();
}

void String::IncRef()
{
	if (!_data) {
		return;
	}

	_data->RefCount += 1;
}

void String::FreeRef()
{
	if (!_data) {
		return;
	}

	_data->RefCount -= 1;
	
	if (_data->RefCount == 0) {
		delete[] _data->Data;
		delete _data;
	};

	_data = 0;
}

void String::MakeExclusive()
{
	if (_data->RefCount > 1) {
		Data *data = new Data;
		data->RefCount = 1;
		data->Length = _data->Length;
		data->Reserved = _data->Reserved;
		data->Data = new char[_data->Reserved];
		memcpy(data->Data, _data->Data, _data->Length + 1);

		FreeRef();
		_data = data;
	}
}
