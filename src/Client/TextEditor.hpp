#ifndef _TEXT_EDITOR_HPP
#define _TEXT_EDITOR_HPP

#include "../Common/UTF8.hpp"

class TextEditor
{
public:
	TextEditor();
	~TextEditor();

	void SetPosition(int fromY, int toY, int fromX, int toX);
	void Redraw();

	String GetText();

	void GoLeft();
	void GoRight();
	void GoUp();
	void GoDown();

	void AddChar(int event);

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
			NlFirst = false;
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

	Line *_firstLine;
	Line *_currentLine;

	void Init();
	void RebuildLines(Line *firstLine);
};

#endif
