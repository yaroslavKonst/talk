#ifndef _MY_STRING_HPP
#define _MY_STRING_HPP

#include "CowBuffer.hpp"

class String
{
public:
	String();
	String(const String &s);
	String(const char *s);
	String(const char *s, int length);
	~String();

	void Clear();

	String &operator=(const String &s);

	String operator+(const String &s) const;
	void operator+=(const String &s);
	void operator+=(char c);

	const char *CStr() const;
	int Length() const;

	bool operator==(const String &s) const;
	bool operator!=(const String &s) const;
	bool operator<(const String &s) const;

	CowBuffer<String> Split(char delim, bool removeEmpty) const;
	String Trim() const;
	String Substring(int start, int length) const;
	String Replace(char from, char to) const;

	String ToLowerCase() const;

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

inline String operator+(const char *str1, const String str2)
{
	return String(str1) + str2;
}

inline String ToString(long value)
{
	if (value == 0) {
		return "0";
	}

	String res;
	long digits = 0;

	long val = value;

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
		long digit = val;

		for (long i = 1; i < digits; i++) {
			digit /= 10;
		}

		digit = digit % 10;

		res += '0' + digit;
		--digits;
	}

	return res;
}

inline bool IsSpace(char c)
{
	return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

String IPToString(int32_t ip);

inline String DataSizeToString(uint64_t dataSize)
{
	String units = "B";

	if (dataSize < 1024) {
		return ToString(dataSize) + " " + units;
	}

	units = "KB";
	uint64_t fraction = (dataSize % 1024) * 10 / 1024;
	dataSize /= 1024;

	if (dataSize < 1024) {
		return ToString(dataSize) + "." + ToString(fraction) +
			" " + units;
	}

	units = "MB";
	fraction = (dataSize % 1024) * 10 / 1024;
	dataSize /= 1024;

	if (dataSize < 1024) {
		return ToString(dataSize) + "." + ToString(fraction) +
			" " + units;
	}

	units = "GB";
	fraction = (dataSize % 1024) * 10 / 1024;
	dataSize /= 1024;

	return ToString(dataSize) + "." + ToString(fraction) + " " + units;
}

inline String TimeSpanInSecondsToString(int64_t timeSpan)
{
	const int64_t minute = 60;
	const int64_t hour = minute * 60;
	const int64_t day = hour * 24;
	const int64_t month = day * 31;
	const int64_t year = day * 365;

	if (timeSpan == 0) {
		return "0 seconds";
	}

	String result;

	bool negative = false;

	if (timeSpan < 0) {
		timeSpan = -timeSpan;
		negative = true;
	}

	if (timeSpan >= year) {
		int64_t years = timeSpan / year;
		result += ToString(years) + " year" + (years > 1 ? "s" : "");
		timeSpan = timeSpan % year;
	}

	if (timeSpan >= month) {
		if (result.Length()) {
			result += " ";
		}

		int64_t months = timeSpan / month;
		result += ToString(months) + " month" + (months > 1 ? "s" : "");
		timeSpan = timeSpan % month;
	}

	if (timeSpan >= day) {
		if (result.Length()) {
			result += " ";
		}

		int64_t days = timeSpan / day;
		result += ToString(days) + " day" + (days > 1 ? "s" : "");
		timeSpan = timeSpan % day;
	}

	if (timeSpan >= hour) {
		if (result.Length()) {
			result += " ";
		}

		int64_t hours = timeSpan / hour;
		result += ToString(hours) + " hour" + (hours > 1 ? "s" : "");
		timeSpan = timeSpan % hour;
	}

	if (timeSpan >= minute) {
		if (result.Length()) {
			result += " ";
		}

		int64_t minutes = timeSpan / minute;
		result += ToString(minutes) + " minute" +
			(minutes > 1 ? "s" : "");
		timeSpan = timeSpan % minute;
	}

	if (timeSpan > 0) {
		if (result.Length()) {
			result += " ";
		}

		result += ToString(timeSpan) + " second" +
			(timeSpan > 1 ? "s" : "");
	}

	if (negative) {
		result = "-" + result;
	}

	return result;
}

#endif
