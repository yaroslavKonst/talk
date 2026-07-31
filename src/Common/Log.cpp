#include "Log.hpp"

#include <ctime>
#include <cstdio>

static bool AllowMultilineLogValue = false;

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

void Log(String section, String message)
{
	int64_t timestamp = GetUnixTime();
	String timeStr = ctime(&timestamp);
	timeStr = "[" + timeStr.Substring(0, timeStr.Length() - 1) + "]: " +
		section + ": ";

	if (AllowMultilineLogValue) {
		String filler;

		for (int i = 0; i < timeStr.Length(); i++) {
			filler += " ";
		}

		int limit = 80 - timeStr.Length() - 1;

		bool tooLongHeader = false;

		if (limit < 20) {
			// Header is too long for nice formatting.
			limit = 39;
			tooLongHeader = true;

			// Header length is at least 60, so it is safe.
			filler = filler.Substring(0, 40);
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

void AllowMultilineLog(bool allow)
{
	AllowMultilineLogValue = allow;
}
