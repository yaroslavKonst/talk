#include "TextEditor.hpp"

#include <curses.h>

#include "UiHelpers.hpp"

static bool IsWhitespace(uint32_t c)
{
	return c == ' ' || c == '\t' || c == '\n';
}

TextEditor::TextEditor()
{
	_currentWord = nullptr;
	_currentChar = 0;

	_currentLine = nullptr;

	_fromY = 0;
	_toY = 0;
	_fromX = 0;
	_toX = 0;
}

TextEditor::~TextEditor()
{
	FreeLines(nullptr);
	FreeWords();
}

void TextEditor::FreeLines(Line *line)
{
	if (!line) {
		line = FirstLine();
	}

	if (!line) {
		return;
	}

	if (line->Previous) {
		line->Previous->Next = nullptr;
	}

	while (line) {
		Line *tmp = line;
		line = line->Next;
		delete tmp;
	}
}

void TextEditor::FreeWords()
{
	Word *word = FirstWord();

	while (word) {
		Word *tmp = word;
		word = word->Next;
		delete tmp;
	}

	_currentWord = nullptr;
}

TextEditor::Word *TextEditor::FirstWord()
{
	if (!_currentWord) {
		return nullptr;
	}

	Word *word = _currentWord;

	while (word->Previous) {
		word = word->Previous;
	}

	return word;
}

TextEditor::Line *TextEditor::FirstLine()
{
	if (!_currentLine) {
		return nullptr;
	}

	Line *line = _currentLine;

	while (line->Previous) {
		line = line->Previous;
	}

	return line;
}

void TextEditor::SetPosition(int fromY, int toY, int fromX, int toX)
{
	_fromY = fromY;
	_toY = toY;
	_fromX = fromX;
	_toX = toX;

	RebuildLines(nullptr);
}

void TextEditor::Normalize()
{
	if (!_currentWord) {
		_currentChar = 0;
		return;
	}

	while (_currentWord->Next &&
		_currentChar == _currentWord->Data.Size())
	{
		_currentWord = _currentWord->Next;
		_currentChar = 0;
	}
}

String TextEditor::GetText()
{
	String text;

	Word *word = FirstWord();

	while (word) {
		if (word->Data.Size()) {
			text += UTF8::Encode(word->Data);
		}

		word = word->Next;
	}

	return text;
}

void TextEditor::RebuildLines(Line *firstLine)
{
	Line *currentLine = firstLine;

	if (!currentLine) {
		currentLine = FirstLine();
	}

	if (currentLine) {
		if (currentLine->Previous) {
			currentLine = currentLine->Previous;
		} else {
			currentLine = nullptr;
		}
	}

	FreeLines(firstLine);

	Word *currentWord;
	int currentCharInWord;

	if (!currentLine) {
		currentWord = FirstWord();
		currentCharInWord = 0;

		if (!currentWord) {
			return;
		}

		currentLine = new Line;
		currentLine->Data = currentWord;
	} else {
		currentWord = currentLine->Data;
		currentCharInWord = currentLine->Offset;
	}

	int width = _toX - _fromX + 1;

	if (width <= 0) {
		_currentLine = currentLine;
		return;
	}

	int currentLineLength = 0;

	bool enforceNewLine = false;

	while (currentWord) {
		bool canPlaceWordOnTheCurrentLine = !enforceNewLine;
		enforceNewLine = false;

		if (canPlaceWordOnTheCurrentLine) {
			bool wordFitsToLine =
				(int)currentWord->Data.Size() -
				currentCharInWord +
				currentLineLength < width;

			if (!wordFitsToLine && currentLineLength > 0) {
				canPlaceWordOnTheCurrentLine = false;
			}
		}

		bool cursorIsOnCurrentSegment =
			currentWord == _currentWord &&
			(int)_currentChar >= currentCharInWord &&
			(int)_currentChar <= currentCharInWord + width;

		if (canPlaceWordOnTheCurrentLine) {
			bool wordFits = (int)currentWord->Data.Size() -
				currentCharInWord <=
				width - currentLineLength;

			if (!wordFits) {
				if (cursorIsOnCurrentSegment) {
					_currentLine = currentLine;
				}

				currentLine->Next = new Line;
				currentLine->Next->Previous =
					currentLine;
				currentLine = currentLine->Next;

				currentLine->Data = currentWord;
				currentLine->Offset = currentCharInWord + width;
				currentCharInWord += width;
				currentLineLength = 0;
			} else {
				if (cursorIsOnCurrentSegment) {
					_currentLine = currentLine;
				}

				uint32_t lastCharInWord = currentWord->Data[
					currentWord->Data.Size() - 1];

				if (lastCharInWord == '\n') {
					enforceNewLine = true;
				}

				currentLineLength += currentWord->Data.Size() -
					currentCharInWord;

				currentWord = currentWord->Next;
				currentCharInWord = 0;
			}
		} else {
			currentLine->Next = new Line;
			currentLine->Next->Previous =
				currentLine;
			currentLine = currentLine->Next;

			currentLine->Data = currentWord;
			currentLine->Offset = 0;
			currentCharInWord = 0;
			currentLineLength = 0;
		}
	}

	if (enforceNewLine) {
		currentWord = _currentWord;

		while (currentWord->Next) {
			currentWord = currentWord->Next;
		}

		currentLine->Next = new Line;
		currentLine->Next->Previous =
			currentLine;
		currentLine = currentLine->Next;

		currentLine->Data = currentWord;
		currentLine->Offset = currentWord->Data.Size();
	}
}

void TextEditor::LocateCursor(Line *&line, int &column)
{
	Line *l = FirstLine();

	while (l) {
		Word *w = l->Data;
		uint64_t o = l->Offset;

		bool hasEnd = l->Next != nullptr;
		Word *endWord = hasEnd ? l->Next->Data : nullptr;
		uint64_t endOffset = hasEnd ? (uint64_t)l->Next->Offset : 0;

		int col = 0;

		for (;;) {
			if (hasEnd && w == endWord && o == endOffset) {
				break;
			}

			if (w == _currentWord && o == _currentChar) {
				line = l;
				column = col;
				_currentLine = l;
				return;
			}

			if (!w) {
				break;
			}

			if (o >= w->Data.Size()) {
				if (!w->Next) {
					break;
				}

				w = w->Next;
				o = 0;
				continue;
			}

			if (w->Data[o] != '\n') {
				++col;
			}

			++o;
		}

		if (!hasEnd && w == _currentWord && o == _currentChar) {
			line = l;
			column = col;
			_currentLine = l;
			return;
		}

		l = l->Next;
	}

	line = _currentLine;
	column = 0;
}

void TextEditor::MoveCursorToColumn(Line *line, int column)
{
	Word *w = line->Data;
	uint64_t o = line->Offset;

	bool hasEnd = line->Next != nullptr;
	Word *endWord = hasEnd ? line->Next->Data : nullptr;
	uint64_t endOffset = hasEnd ? (uint64_t)line->Next->Offset : 0;

	int col = 0;

	for (;;) {
		if (hasEnd && w == endWord && o == endOffset) {
			break;
		}

		if (!w) {
			break;
		}

		if (o >= w->Data.Size()) {
			if (!w->Next) {
				break;
			}

			w = w->Next;
			o = 0;
			continue;
		}

		if (col >= column) {
			break;
		}

		if (w->Data[o] == '\n') {
			break;
		}

		++o;
		++col;
	}

	_currentWord = w;
	_currentChar = o;

	Normalize();
}

String TextEditor::RenderLine(Line *line)
{
	Word *w = line->Data;
	uint64_t o = line->Offset;

	bool hasEnd = line->Next != nullptr;
	Word *endWord = hasEnd ? line->Next->Data : nullptr;
	uint64_t endOffset = hasEnd ? (uint64_t)line->Next->Offset : 0;

	int width = _toX - _fromX + 1;

	if (width < 1) {
		width = 1;
	}

	CowBuffer<uint32_t> out;
	int count = 0;

	while (w) {
		if (hasEnd && w == endWord && o == endOffset) {
			break;
		}

		if (o >= w->Data.Size()) {
			if (!w->Next) {
				break;
			}

			w = w->Next;
			o = 0;
			continue;
		}

		uint32_t c = w->Data[o];

		if (c != '\n') {
			CowBuffer<uint32_t> one(1);
			one[0] = (c == '\t') ? ' ' : c;
			out = out.Concat(one);

			if (++count >= width) {
				break;
			}
		}

		++o;
	}

	return UTF8::Encode(out);
}

void TextEditor::Redraw()
{
	UiHelpers::ClearScreen(_fromY, _toY, _fromX, _toX);

	Line *curLine = nullptr;
	int curColumn = 0;
	LocateCursor(curLine, curColumn);

	int height = _toY - _fromY + 1;
	int width = _toX - _fromX + 1;

	if (height < 1 || width < 1) {
		move(_fromY, _fromX);
		return;
	}

	if (!curLine) {
		move(_fromY, _fromX);
		return;
	}

	Line *top = curLine;
	Line *bottom = curLine;

	int size = 1;

	while (size < height) {
		bool grow = false;

		if (top->Previous) {
			top = top->Previous;
			++size;
			grow = true;
		}

		if (size >= height) {
			break;
		}

		if (bottom->Next) {
			bottom = bottom->Next;
			++size;
			grow = true;
		}

		if (!grow) {
			break;
		}
	}

	int row = _fromY;
	int cursorRow = _fromY;

	Line *line = top;

	while (line && row <= _toY) {
		String s = RenderLine(line);

		move(row, _fromX);
		addstr(s.CStr());

		if (line == curLine) {
			cursorRow = row;
		}

		line = line->Next;
		++row;
	}

	int cursorX = _fromX + curColumn;

	if (cursorX > _toX) {
		cursorX = _toX;
	}

	move(cursorRow, cursorX);
}

bool TextEditor::GoLeft()
{
	if (!_currentWord) {
		_currentChar = 0;
		return false;
	}

	if (_currentChar > 0) {
		--_currentChar;
		return true;
	}

	Word *previous = _currentWord->Previous;

	while (previous && previous->Data.Size() == 0) {
		previous = previous->Previous;
	}

	if (!previous) {
		return false;
	}

	_currentWord = previous;
	_currentChar = previous->Data.Size() - 1;

	return true;
}

bool TextEditor::GoRight()
{
	if (!_currentWord) {
		_currentChar = 0;
		return false;
	}

	Word *oldWord = _currentWord;
	uint32_t oldChar = _currentChar;

	if (_currentChar < _currentWord->Data.Size()) {
		++_currentChar;
	}

	Normalize();

	return _currentWord != oldWord || _currentChar != oldChar;
}

bool TextEditor::GoUp()
{
	if (!_currentWord) {
		_currentChar = 0;
		return false;
	}

	Line *line = nullptr;
	int column = 0;
	LocateCursor(line, column);

	if (!line || !line->Previous) {
		return false;
	}

	MoveCursorToColumn(line->Previous, column);
	return true;
}

bool TextEditor::GoDown()
{
	if (!_currentWord) {
		_currentChar = 0;
		return false;
	}

	Line *line = nullptr;
	int column = 0;
	LocateCursor(line, column);

	if (!line || !line->Next) {
		return false;
	}

	MoveCursorToColumn(line->Next, column);
	return true;
}

void TextEditor::InsertChar(uint32_t c)
{
	if (!_currentWord) {
		_currentWord = new Word();
		_currentChar = 0;
	}

	uint64_t cwLen = _currentWord->Data.Size();

	CowBuffer<uint32_t> one(1);
	one[0] = c;

	if (!IsWhitespace(c)) {
		bool newWordIsRequired =
			_currentWord->Data.Size() &&
			_currentChar == _currentWord->Data.Size() &&
			IsWhitespace(_currentWord->Data[_currentChar - 1]);

		if (newWordIsRequired) {
			Word *w = new Word;
			w->Data = one;
			w->Previous = _currentWord;
			w->Next = _currentWord->Next;

			if (_currentWord->Next) {
				_currentWord->Next->Previous = w;
			}

			_currentWord->Next = w;

			_currentWord = w;
			_currentChar = 1;
		} else {
			_currentWord->Data =
				_currentWord->Data.Slice(0, _currentChar)
				.Concat(one)
				.Concat(_currentWord->Data.Slice(
					_currentChar,
					cwLen - _currentChar));

			_currentChar += 1;
		}

		Normalize();
		RebuildLines(_currentLine);
		return;
	}

	// A whitespace character ends the current word. Everything after the
	// cursor becomes the following word (it is already a valid word: a
	// sequence of non-whitespace with the original separator at its end).

	CowBuffer<uint32_t> right =
		_currentWord->Data.Slice(_currentChar, cwLen - _currentChar);

	_currentWord->Data = _currentWord->Data.Slice(0, _currentChar);

	bool canAddCharToWord =
		!_currentWord->Next &&
		_currentWord->Data.Size() &&
		!IsWhitespace(_currentWord->Data[
			_currentWord->Data.Size() - 1]);

	if (canAddCharToWord) {
		_currentWord->Data = _currentWord->Data.Concat(one);
		++_currentChar;
	} else {
		Word *w = new Word;
		w->Data = one;
		w->Previous = _currentWord;
		w->Next = _currentWord->Next;

		if (_currentWord->Next) {
			_currentWord->Next->Previous = w;
		}

		_currentWord->Next = w;
		_currentWord = w;
		_currentChar = 0;
	}

	if (right.Size()) {
		Word *w = new Word;
		w->Data = right;
		w->Previous = _currentWord;
		w->Next = _currentWord->Next;

		if (_currentWord->Next) {
			_currentWord->Next->Previous = w;
		}

		_currentWord = w;
		_currentChar = 0;
	}

	Normalize();
	RebuildLines(_currentLine);
}

void TextEditor::DeleteChar()
{
	if (!_currentWord) {
		return;
	}

	if (_currentChar >= _currentWord->Data.Size()) {
		return;
	}

	if (_currentChar < _currentWord->Data.Size() - 1) {
		_currentWord->Data =
			_currentWord->Data.Slice(0, _currentChar).Concat(
				_currentWord->Data.Slice(
					_currentChar + 1,
					_currentWord->Data.Size() -
						_currentChar - 1));

		RemoveIfEmpty(_currentWord);
		Normalize();
		RebuildLines(_currentLine);
		return;
	}

	_currentWord->Data = _currentWord->Data.Slice(0, _currentChar);

	if (_currentWord->Next) {
		_currentWord->Data = _currentWord->Data.Concat(
			_currentWord->Next->Data);

		Word *tmp = _currentWord->Next;

		if (tmp->Next) {
			tmp->Next->Previous = _currentWord;
		}

		_currentWord->Next = tmp->Next;

		delete tmp;
	}

	RemoveIfEmpty(_currentWord);

	Normalize();
	RebuildLines(_currentLine);
}

void TextEditor::RemoveIfEmpty(Word *word)
{
	if (word->Data.Size() != 0) {
		return;
	}

	if (!word->Previous && !word->Next) {
		FreeLines(nullptr);
		FreeWords();
		_currentWord = nullptr;
		_currentLine = nullptr;
		_currentChar = 0;
		return;
	}

	Word *target;
	uint64_t position;

	if (word->Previous) {
		target = word->Previous;
		position = target->Data.Size();
	} else {
		target = word->Next;
		position = 0;
	}

	if (word->Previous) {
		word->Previous->Next = word->Next;
	}

	if (word->Next) {
		word->Next->Previous = word->Previous;
	}

	if (_currentWord == word) {
		_currentWord = target;
		_currentChar = position;
	}

	delete word;
}

void TextEditor::AddChar(int event)
{
	if (event < 0 || event > 0xff) {
		if (event == KEY_DC) {
			DeleteChar();
		}

		return;
	}

	_utf8Decoder.AddByte(event);

	if (!_utf8Decoder.HasChar()) {
		return;
	}

	uint32_t c = _utf8Decoder.GetChar();
	_utf8Decoder.Reset();

	if (c == '\b') {
		if (GoLeft()) {
			DeleteChar();
		}
	} else if (c == KEY_DC) {
		DeleteChar();
	} else {
		InsertChar(c);
	}

	RebuildLines(_currentLine);
}

void TextEditor::Split(TextEditor &editor)
{
	// Erase the target editor's current contents.
	editor.FreeLines(nullptr);
	editor.FreeWords();
	editor._currentWord = nullptr;
	editor._currentLine = nullptr;
	editor._currentChar = 0;

	if (!_currentWord) {
		return;
	}

	Normalize();

	Word *w = _currentWord;
	uint64_t p = _currentChar;
	uint64_t n = w->Data.Size();

	CowBuffer<uint32_t> rightData = w->Data.Slice(p, n - p);
	Word *tail = w->Next;

	w->Data = w->Data.Slice(0, p);
	w->Next = nullptr;

	_currentWord = w;
	_currentChar = w->Data.Size();

	if (rightData.Size() || tail) {
		Word *head = new Word;
		head->Data = rightData;
		head->Previous = nullptr;
		head->Next = tail;

		if (tail) {
			tail->Previous = head;
		}

		editor._currentWord = head;
		editor._currentChar = 0;

		editor.RemoveIfEmpty(head);
	}

	RemoveIfEmpty(w);

	FreeLines(nullptr);
	_currentLine = nullptr;

	if (_currentWord) {
		Normalize();
	}

	RebuildLines(nullptr);

	editor.FreeLines(nullptr);
	editor._currentLine = nullptr;

	if (editor._currentWord) {
		editor.Normalize();
	}

	editor.RebuildLines(nullptr);
}

void TextEditor::Merge(TextEditor &editor)
{
	if (!editor._currentWord) {
		return;
	}

	Word *head = editor.FirstWord();

	editor.FreeLines(nullptr);
	editor._currentWord = nullptr;
	editor._currentLine = nullptr;
	editor._currentChar = 0;

	if (!_currentWord) {
		_currentWord = head;
		_currentChar = 0;
	} else {
		Word *last = _currentWord;

		while (last->Next) {
			last = last->Next;
		}

		bool lastHasSeparator =
			last->Data.Size() &&
			IsWhitespace(last->Data[last->Data.Size() - 1]);

		if (lastHasSeparator) {
			last->Next = head;
			head->Previous = last;

			_currentWord = head;
			_currentChar = 0;
		} else {
			uint64_t mergePos = last->Data.Size();

			last->Data = last->Data.Concat(head->Data);
			last->Next = head->Next;

			if (head->Next) {
				head->Next->Previous = last;
			}

			delete head;

			_currentWord = last;
			_currentChar = mergePos;
		}
	}

	FreeLines(nullptr);
	_currentLine = nullptr;
	Normalize();
	RebuildLines(nullptr);
}
