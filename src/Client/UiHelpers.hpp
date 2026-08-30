#ifndef _UI_HELPERS_HPP
#define _UI_HELPERS_HPP

#include "../Common/MyString.hpp"
#include "../Common/UTF8.hpp"

namespace UiHelpers
{
	void ClearScreen(int fromY, int toY, int fromX, int toX);

	void DrawFrame(
		int fromY,
		int toY,
		int fromX,
		int toX,
		String caption,
		int color,
		bool &runningCaption);

	void DrawSelector(
		String name,
		String value,
		String upKey,
		String downKey,
		int y,
		int x);

	CowBuffer<String> MakeMultiline(String text, int limit);

	class TextBox
	{
	public:
		TextBox();

		String Caption;

		bool HasText();
		int GetTextLength();
		String GetText();
		void SetText(String text);

		void SetCaptionPosition(int y, int x);
		void SetTextPosition(int y, int x);
		void SetWidthLimit(int limit);
		void AlignTextToCaption();

		void Redraw();
		void SetCursor();

		void ProcessChar(int event);

	private:
		int _cX;
		int _cY;
		int _tX;
		int _tY;

		int _widthLimit;

		CowBuffer<uint32_t> _text;

		UTF8::Decoder _decoder;
	};

	String GetRunningLine(String text, int widthLimit, bool &running);
	bool DrawRunningLine(String text, int widthLimit);
	void UpdateRunningLineSeed();

	bool DrawCommentedLine(
		String text,
		String comment,
		int width,
		int textAttr,
		int commentAttr);
}

#endif
