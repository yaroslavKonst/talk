#include "UiHelpers.hpp"

#include <curses.h>

#include "TextColor.hpp"
#include "../Common/UTF8.hpp"

using namespace UiHelpers;

static uint32_t RunningLineSeedValue = 0;

void UiHelpers::ClearScreen(int fromY, int toY, int fromX, int toX)
{
	for (int r = fromY; r <= toY; r++) {
		for (int c = fromX; c <= toX; c++) {
			move(r, c);
			addch(' ');
		}
	}
}

void UiHelpers::DrawFrame(
	int fromY,
	int toY,
	int fromX,
	int toX,
	String caption,
	int color,
	bool &runningCaption)
{
	attrset(color);

	for (int r = fromY + 1; r < toY; r++) {
		move(r, fromX);
		addch(ACS_VLINE);

		move(r, toX);
		addch(ACS_VLINE);
	}

	for (int c = fromX + 1; c < toX; c++) {
		move(fromY, c);
		addch(ACS_HLINE);

		move(toY, c);
		addch(ACS_HLINE);
	}

	move(fromY, fromX);
	addch(ACS_ULCORNER);
	move(fromY, toX);
	addch(ACS_URCORNER);
	move(toY, fromX);
	addch(ACS_LLCORNER);
	move(toY, toX);
	addch(ACS_LRCORNER);

	move(fromY, fromX + 1);
	runningCaption = DrawRunningLine(caption, toX - fromX - 1);

	attrset(COLOR_PAIR(DEFAULT_TEXT));
}

void UiHelpers::DrawSelector(
	String name,
	String value,
	String upKey,
	String downKey,
	int y,
	int x)
{
	move(y - 4, x - name.Length() / 2);
	addstr(name.CStr());
	move(y - 2, x - upKey.Length() / 2);
	addstr(upKey.CStr());
	move(y, x - value.Length() / 2);
	addstr(value.CStr());
	move(y + 2, x - downKey.Length() / 2);
	addstr(downKey.CStr());
}

CowBuffer<String> UiHelpers::MakeMultiline(String text, int limit)
{
	struct Line
	{
		Line *Next;
		String Line;
	};

	Line *first = nullptr;
	Line **last = &first;

	int lineCount = 0;
	bool endWithNewLine = false;

	while (text.Length() > 0) {
		String line;
		bool hasNewLine = false;

		endWithNewLine = false;

		for (int i = 0; i < text.Length(); i++) {
			if (text.CStr()[i] == '\n') {
				hasNewLine = true;
				break;
			}
		}

		if (!hasNewLine && UTF8::StrLen(text.CStr()) < limit) {
			line = text;
			text.Clear();
		} else {
			int position = 0;
			int charIndex = 0;

			while (charIndex < limit - 1) {
				char currentChar = text.CStr()[position];

				if (currentChar == '\n') {
					endWithNewLine = true;
					break;
				}

				++position;

				if (!UTF8::IsTrailing(currentChar)) {
					++charIndex;
				}
			}

			while (position >= 0) {
				char currentChar = text.CStr()[position];

				if (IsSpace(currentChar)) {
					break;
				}

				--position;

				if (!UTF8::IsTrailing(currentChar)) {
					--charIndex;
				}
			}

			if (position < 0) {
				charIndex = 0;
				position = 0;

				while (charIndex < limit - 1)
				{
					if (position >= text.Length() - 1) {
						break;
					}

					char currentChar =
						text.CStr()[position];

					++position;

					if (!UTF8::IsTrailing(currentChar)) {
						++charIndex;
					}
				}

				while (UTF8::IsTrailing(text.CStr()[position]))
				{
					++position;
				}
			}

			if (position == 0) {
				text = text.Substring(
					1,
					text.Length() - 1);
			} else if (!IsSpace(text.CStr()[position])) {
				line = text.Substring(0, position);
				text = text.Substring(
					position,
					text.Length() - position);
			} else {
				line = text.Substring(0, position);
				text = text.Substring(
					position + 1,
					text.Length() - position - 1);
			}
		}

		*last = new Line;
		(*last)->Next = nullptr;
		(*last)->Line = line;

		last = &((*last)->Next);
		++lineCount;
	}

	if (endWithNewLine) {
		*last = new Line;
		(*last)->Next = nullptr;
		++lineCount;
	}

	CowBuffer<String> result(lineCount);

	int index = 0;

	while (first) {
		result[index] = first->Line;
		++index;

		Line *tmp = first;
		first = first->Next;
		delete tmp;
	}

	return result;
}

// Text box.
TextBox::TextBox()
{
	_cX = 0;
	_cY = 0;
	_tX = 0;
	_tY = 0;
	_widthLimit = -1;
}

void TextBox::SetCaptionPosition(int y, int x)
{
	_cY = y;
	_cX = x;
}

void TextBox::SetTextPosition(int y, int x)
{
	_tY = y;
	_tX = x;
}

void TextBox::SetWidthLimit(int limit)
{
	_widthLimit = limit;
}

void TextBox::AlignTextToCaption()
{
	_tY = _cY;
	_tX = _cX + Caption.Length();
}

void TextBox::Redraw()
{
	move(_cY, _cX);
	addstr(Caption.CStr());
	move(_tY, _tX);

	String text = Text;

	int prefixSize = _tX - _cX;

	if (prefixSize < 0) {
		prefixSize = 0;
	}

	bool adjustStringWidth =
		_widthLimit != -1 &&
		text.Length() > _widthLimit - prefixSize;

	if (adjustStringWidth) {
		int offset = text.Length() - _widthLimit + prefixSize;
		text = text.Substring(offset, text.Length() - offset);
	}

	addstr(text.CStr());
}

void TextBox::SetCursor()
{
	int prefixSize = _tX - _cX;

	if (prefixSize < 0) {
		prefixSize = 0;
	}

	bool adjustPosition =
		_widthLimit != -1 &&
		Text.Length() > _widthLimit - prefixSize;

	int textWidth = Text.Length();

	if (adjustPosition) {
		textWidth = _widthLimit - prefixSize;
	}

	move(_tY, _tX + textWidth);
}

void TextBox::ProcessChar(int event)
{
	if (event == '\b') {
		if (Text.Length() == 0) {
			return;
		}

		Text = Text.Substring(0, Text.Length() - 1);
		return;
	}

	Text += event;
}

String UiHelpers::GetRunningLine(String text, int widthLimit, bool &running)
{
	if (text.Length() <= widthLimit) {
		running = false;
		return text;
	}

	int len = text.Length();

	text = text + "       " + text;
	int maxOffset = len + 7;
	int offset = RunningLineSeedValue % maxOffset;

	text = text.Substring(offset, widthLimit);

	running = true;
	return text;
}

bool UiHelpers::DrawRunningLine(String text, int widthLimit)
{
	bool result;
	text = GetRunningLine(text, widthLimit, result);
	addstr(text.CStr());
	return result;
}

void UiHelpers::UpdateRunningLineSeed()
{
	++RunningLineSeedValue;

	if (RunningLineSeedValue > 1024 * 1024 * 1024) {
		RunningLineSeedValue = 0;
	}
}
