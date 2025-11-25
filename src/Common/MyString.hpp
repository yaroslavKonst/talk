#ifndef _MY_STRING_HPP
#define _MY_STRING_HPP

#include "CowBuffer.hpp"

class String
{
public:
	String();
	String(const String &s);
	String(const char *s);
	~String();

	void Clear();

	String &operator=(const String &s);

	String operator+(const String &s) const;
	void operator+=(const String &s);
	void operator+=(char c);

	const char *CStr() const;
	int Length() const;

	bool operator==(const String &s) const;
	bool operator<(const String &s) const;

	CowBuffer<String> Split(char delim, bool removeEmpty) const;
	String Trim() const;
	String Substring(int start, int length) const;
	String Replace(char from, char to) const;

	void Wipe();

private:
	struct Data
	{
		int RefCount;
		int Length;
		int Reserved;
		char *Data;
	};

	Data *_data;

	String(Data *data);

	void IncRef();
	void FreeRef();

	void MakeExclusive();
};

inline String ToString(int value)
{
	if (value == 0) {
		return String("0");
	}

	String res;
	int digits = 0;

	int val = value;

	if (val < 0) {
		res += '-';
		val = -val;
	}

	while (val) {
		++digits;
		val /= 10;
	}

	val = value;

	if (val < 0) {
		val = -val;
	}

	while (digits) {
		int digit = val;

		for (int i = 1; i < digits; i++) {
			digit /= 10;
		}

		digit = digit % 10;

		res += '0' + digit;
		--digits;
	}

	return res;
}

#endif
