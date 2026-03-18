#ifndef _UTF8_HPP
#define _UTF8_HPP

#include "../Common/CowBuffer.hpp"
#include "../Common/MyString.hpp"

namespace UTF8
{
	inline bool IsTrailing(char c)
	{
		return (c & 0xc0) == 0x80;
	}

	inline int StrLen(const char *str)
	{
		int length = 0;

		while (*str) {
			if (!IsTrailing(*str)) {
				++length;
			}

			++str;
		}

		return length;
	}

	CowBuffer<uint32_t> Decode(String str);
	String Encode(const CowBuffer<uint32_t> str);
	String Encode(uint32_t c);

	class Decoder
	{
	public:
		Decoder();

		void Reset();

		bool HasChar();
		uint32_t GetChar();

		bool AddByte(int c);

	private:
		uint32_t _expectedBytes;
		uint32_t _value;
	};
}

#endif
