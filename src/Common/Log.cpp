#include "Log.hpp"

#include <cstdio>

#include "UnixTime.hpp"
#include "Exception.hpp"

static int MaxLogWidth = 80;
static LogLevel LogLevelValue = LogLevel::Verbose;

static CowBuffer<String> MakeMultiline(String text, int limit)
{
	struct Line
	{
		Line *Next;
		String Text;

		Line(String text)
		{
			Next = nullptr;
			Text = text;
		}
	};

	Line *firstLine = nullptr;
	Line *lastLine = nullptr;
	int lineCount = 0;

	while (text.Length()) {
		if (text.Length() < limit) {
			// One line left.
			Line *line = new Line(text);
			text = String();

			if (!firstLine) {
				firstLine = line;
				lastLine = line;
			} else {
				lastLine->Next = line;
				lastLine = line;
			}

			++lineCount;
		} else {
			// Line split is required.
			int splitPosition = limit;

			while (
				splitPosition > 0 &&
				text.CStr()[splitPosition] != ' ')
			{
				--splitPosition;
			}

			if (!splitPosition) {
				splitPosition = limit;
			} else {
				splitPosition += 1;
			}

			Line *line = new Line(text.Substring(0, splitPosition));
			text = text.Substring(
				splitPosition,
				text.Length() - splitPosition);

			if (!firstLine) {
				firstLine = line;
				lastLine = line;
			} else {
				lastLine->Next = line;
				lastLine = line;
			}

			++lineCount;
		}
	}

	if (!lineCount) {
		return CowBuffer<String>();
	}

	CowBuffer<String> result(lineCount);

	int index = 0;

	while (firstLine) {
		result[index] = firstLine->Text;
		++index;

		Line *tmp = firstLine;
		firstLine = firstLine->Next;
		delete tmp;
	}

	return result;
}

void Log(LogLevel level, String section, String message)
{
	if (level < LogLevelValue) {
		return;
	}

	String timeStr = "[" + TimeInSecondsToString(GetUnixTime()) + "]: " +
		section + ": ";

	if (MaxLogWidth > 0) {
		String filler;

		int limit = MaxLogWidth - timeStr.Length() - 1;

		bool tooLongHeader = false;

		if (limit < 20) {
			// Header is too long for nice formatting.
			limit = MaxLogWidth - 40 - 1;
			tooLongHeader = true;
		}

		int fillerWidth = MaxLogWidth - limit - 1;

		for (int i = 0; i < fillerWidth; i++) {
			filler += " ";
		}

		CowBuffer<String> lines = MakeMultiline(message, limit);

		if (tooLongHeader) {
			printf("%s\n", timeStr.CStr());
		}

		for (unsigned int i = 0; i < lines.Size(); i++) {
			if (!i && !tooLongHeader) {
				printf(
					"%s%s\n",
					timeStr.CStr(),
					lines[i].CStr());
			} else {
				printf(
					"%s%s\n",
					filler.CStr(),
					lines[i].CStr());
			}
		}
	} else {
		printf("%s%s\n", timeStr.CStr(), message.CStr());
	}

	fflush(stdout);
}

void SetMaxLogWidth(int width)
{
	int minLogWidth = 70;

	if (width < minLogWidth && width > 0) {
		THROW("Log can not be narrower than " +
			ToString(minLogWidth) + ".");
	}

	MaxLogWidth = width;
}

void SetLogLevel(LogLevel level)
{
	LogLevelValue = level;
}
