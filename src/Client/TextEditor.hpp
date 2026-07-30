#ifndef _TEXT_EDITOR_HPP
#define _TEXT_EDITOR_HPP

#include "../Common/UTF8.hpp"

// Text is represented as a bidirectional linked list. Each node
// contains one word. A word is a sequence of nonwhitespace characters
// with an optional whitespace character in the end. If the whitespace
// character in the end is LF then the next word must be printed on the
// next line.

class TextEditor
{
public:
	TextEditor();
	~TextEditor();

	// Set editor position on screen.
	void SetPosition(int fromY, int toY, int fromX, int toX);

	void Redraw();

	// Characters are stored as 32 bit integers and encoded as UTF8
	// by GetText.
	String GetText();

	// Move cursor.
	bool GoLeft();
	bool GoRight();
	bool GoUp();
	bool GoDown();

	// Process event. Input can be a part of multibyte UTF8 character.
	void AddChar(int event);

	// Split text in current cursor position. All text starting from
	// cursor mosition is moved to 'editor' given as argument.
	// Existing 'editor' contents must be erased.
	void Split(TextEditor &editor);

	// Move all text from 'editor' argument to this instance. New text
	// is appended to existing. Data is moved which means that 'editor'
	// contents are empty after this method call.
	void Merge(TextEditor &editor);

private:
	int _fromY;
	int _toY;
	int _fromX;
	int _toX;

	UTF8::Decoder _utf8Decoder;

	struct Word
	{
		Word *Next;
		Word *Previous;
		CowBuffer<uint32_t> Data;

		Word()
		{
			Next = nullptr;
			Previous = nullptr;
		}
	};

	struct Line
	{
		Line *Next;
		Line *Previous;
		Word *Data;
		int Offset;

		Line()
		{
			Next = nullptr;
			Previous = nullptr;
			Data = nullptr;
			Offset = 0;
		}
	};

	Word *_currentWord;
	uint32_t _currentChar;

	Line *_currentLine;

	void RebuildLines(Line *firstLine);

	void FreeLines(Line *line);
	void FreeWords();

	Word *FirstWord();
	Line *FirstLine();

	// Move the cursor forward across word ends so it never stops after
	// the word end when a next word exists.
	void Normalize();

	void InsertChar(uint32_t c);

	// Delete char on the current cursor position.
	void DeleteChar();
	void RemoveIfEmpty(Word *word);

	void LocateCursor(Line *&line, int &column);
	void MoveCursorToColumn(Line *line, int column);

	String RenderLine(Line *line);
};

#endif
