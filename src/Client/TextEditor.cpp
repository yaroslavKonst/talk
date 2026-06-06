#include "TextEditor.hpp"

#include <curses.h>

#include "UiHelpers.hpp"

TextEditor::TextEditor()
{
	_currentWord = new Word;
}

TextEditor::~TextEditor()
{
	delete _currentWord;
}

void TextEditor::SetPosition(int fromY, int toY, int fromX, int toX)
{
	_fromY = fromY;
	_toY = toY;
	_fromX = fromX;
	_toX = toX;
}

void TextEditor::Redraw()
{
	UiHelpers::ClearScreen(_fromY, _toY, _fromX, _toX);

	String s = UTF8::Encode(_currentWord->Data);
	move(_fromY, _fromX);
	addstr(s.CStr());
}

String TextEditor::GetText()
{
	return UTF8::Encode(_currentWord->Data);
}

void TextEditor::GoLeft()
{
}

void TextEditor::GoRight()
{
}

void TextEditor::GoUp()
{
}

void TextEditor::GoDown()
{
}

void TextEditor::AddChar(int event)
{
	_utf8Decoder.AddByte(event);

	if (!_utf8Decoder.HasChar()) {
		return;
	}

	int32_t c = _utf8Decoder.GetChar();

	if (c == '\b') {
		_currentWord->Data = _currentWord.Slice(
			0,
			_currentWord->Data.Size() - 1);

		return;
	}

	CowBuffer<uint32_t> charBuf(1);
	charBuf[0] = c;

	_currentWord->Data = _currentWord->Data.Concat(charBuf);
}


/*TextEditor::TextEditor()
{
	_fromY = 0;
	_toY = 0;
	_fromX = 0;
	_toX = 0;

	_currentWord = nullptr;
	_currentChar = 0;

	_firstLine = nullptr;
	_currentLine = nullptr;

	Init();
}

TextEditor::~TextEditor
{
	_currentChar = 0;

	if (!_currentWord) {
		return;
	}

	while (_currentWord->Previous) {
		_currentWord = _currentWord->Previous;
	}

	_currentLine = nullptr;

	while (_firstLine) {
		Line *tmp = _firstLine;
		_firstLine = _firstLine->Next;
		delete tmp;
	}

	while (_currentWord) {
		Word *tmp = _currentWord;
		_currentWord = _currentWord->Next;
		delete tmp;
	}
}

void TextEditor::SetPosition(int fromY, int toY, int fromX, int toX)
{
	_fromY = fromY;
	_toY = toY;
	_fromX = fromX;
	_toX = toX;

	RebuildLines(_firstLine);
}

TextEditor::Redraw()
{
	int heightLimit = toY - fromY + 1;
	int widthLimit = toX - fromX + 1;

	UiHelpers::ClearScreen(_fromY, _toY, _fromX, _toX);

	if (!_currentWord) {
		move(_fromY, _fromX);
		return;
	}

	Line *upLine = _currentLine;
	Line *downLine = _currentLine;

	int lineCount = 1;

	bool goUp = true;

	while (lineCount < heightLimit) {
		bool success = false;

		if (goUp || !downLine->Next) {
			goUp = false;

			if (upLine->Previous) {
				upLine = upLine->Previous;
				success = true;
				++lineCount;
			}
		}

		if (success) {
			continue;
		}

		if (!goUp) {
			goUp = true;

			if (downLine->Next) {
				downLine = downLine->Next;
				success = true;
				++lineCount;
			}
		}

		if (!success) {
			break;
		}
	}

	for (;;) {
		move(_fromY + lineCount - 1, _fromX);

		Word *w = downLine->Data;

		for (;;) {
			if (w->Data.Size()) {
				String s = UTF8::Encode(w->Data);

				if (s.CStr()[s.Length() - 1] == '\n') {
					s = s.Substring(0, s.Length() - 1);
				}

				addstr(s.CStr());
			}

			if (!w->Next) {
				break;
			}

			w = w->Next;

			if (downLine->Previous) {
				if (w == downLine->Previous->Data) {
					break;
				}
			}
		}

		--lineCount;

		if (upLine == downLine) {
			break;
		}

		if (!downLine->Previous) {
			break;
		}

		downLine = downLine->Previous;
	}
}

String TextEditor::GetText()
{
	if (!_firstLine) {
		return "";
	}

	String result;

	Word *w = _firstLine->Data;

	while (w) {
		result += UTF8::Encode(w->Data());
		w = w->Next;
	}

	return result;
}

void TextEditor::GoLeft()
{
	if (_currentChar > 0) {
		--_currentChar;
		return;
	}

	if (!_currentWord->Previous) {
		return;
	}

	if (_currentWord == _currentLine->Data) {
		_currentLine = _currentLine->Previous;
	}

	_currentWord = _currentWord->Previous;
}

void TextEditor::GoRight()
{
	if (_currentChar < _currentWord->Data.Size() - 1) {
		++_currentChar;
		return;
	}

	if (!_currentWord->Next) {
		return;
	}

	_currentWord = _currentWord->Next;

	if (_currentLine->Next) {
		if (_currentWord == _currentLine->Next) {
			_currentLine = _currentLine->Next;
		}
	}
}

void TextEditor::GoUp()
{
	THROW("Not implemented.");

	if (!_currentLine->Previous) {
		return;
	}

	int fullOffset = _currentChar;

	Word *w = _currentWord;

	do {
		if (!w->Previous) {
			break;
		}

		w = w->Previous;

		fullOffset += w->Data.Size();
	} while (w != _currentLine->Data)

	_currentLine = _currentLine->Previous;

	_currentWord = _currentLine->Data;

	while (fullOffset > _currentWord->Data.Size()) {
		if (!_currentWord->Next) {
			break;
		}

		if (_currentWord->Next == _currentLine->Next->Data) {
			break;
		}

		fullOffset -= _currentWord->Data.Size();
		_currentWord = _currentWord->Next;
	}

	if (fullOffset > _currentWord.Size()) {
		_currentChar = _currentWord.Size();
	} else {
		_currentChar = fullOffset;
	}
}

void TextEditor::GoDown()
{
	THROW("Not implemented.");

	if (!_currentLine->Next) {
		return;
	}

	int fullOffset = _currentChar;

	Word *w = _currentWord;

	do {
		if (!w->Previous) {
			break;
		}

		w = w->Previous;

		fullOffset += w->Data.Size();
	} while (w != _currentLine->Data)

	_currentLine = _currentLine->Next;

	_currentWord = _currentLine->Data;

	while (fullOffset > _currentWord->Data.Size()) {
		if (!_currentWord->Next) {
			break;
		}

		if (_currentLine->Next) {
			if (_currentWord->Next == _currentLine->Next->Data) {
				break;
			}
		}

		fullOffset -= _currentWord->Data.Size();
		_currentWord = _currentWord->Next;
	}

	if (fullOffset > _currentWord.Size()) {
		_currentChar = _currentWord.Size();
	} else {
		_currentChar = fullOffset;
	}
}

static bool IsSpace(int c)
{
	return c == ' ' || c == '\n' || c == '\t';
}

void TextEditor::AddChar(int event)
{
	_utf8Decoder.AddByte(event);

	if (!_utf8Decoder.HasChar()) {
		return;
	}

	int32_t c = _utf8Decoder.GetChar();

	if (IsSpace(c)) {
		Word *w = new Word;
		w->Next = _currentWord->Next;
		w->Previous = _currentWord;
		w->Data = CowBuffer<uint32_t>(1);
		w->Data[0] = c;

		_currentWord = w;
		_currentChar = 1;
	} else if (c == '\b') {
		if (_currentChar > 0) {
			CowBuffer<uint32_t> head = _currentWord->Data.Slice(
				0,
				_currentChar - 1);

			CowBuffer<uint32_t> tail = _currentWord->Data.Slice(
				_currentChar + 1,
				_currentWord->Data.Size() - _currentChar - 1);

			_currentWord->Data = head.Concat(tail);

			if (!_currentWord->Data.Size()) {
				Word *tmp = _currentWord;

				if (_currentWord->Previous) {
					_currentWord->Next->Previous =
						_currentWord->Previous;
					_currentWord = _currentWord->Previous();
					_currentChar =
						_currentWord->Data.Size();
				}

			GoLeft();
		}
	} else {

	}

	RebuildLines(_currentLine);
}

void TextEditor::Init()
{
	_currentWord = new Word;
	_firstLine = new Line;
	_firstLine->Data = _currentWord;
	_currentLine = _firstLine;
}

void TextEditor::RebuildLines(Line *firstLine)
{
}*/
