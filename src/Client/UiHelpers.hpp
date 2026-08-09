#ifndef _UI_HELPERS_HPP
#define _UI_HELPERS_HPP

#include "../Common/MyString.hpp"

namespace UiHelpers
{
	void ClearScreen(int fromY, int toY, int fromX, int toX);

	void DrawFrame(
		int fromY,
		int toY,
		int fromX,
		int toX,
		String caption,
		int color);

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
		String Text;

		void SetCaptionPosition(int y, int x);
		void SetTextPosition(int y, int x);
		void AlignTextToCaption();

		void Redraw();
		void SetCursor();

		void ProcessChar(int event);

	private:
		int _cX;
		int _cY;
		int _tX;
		int _tY;
	};

	String GetRunningLine(String text, int widthLimit, bool &running);
	bool DrawRunningLine(String text, int widthLimit);
	void UpdateRunningLineSeed();
}

#endif
