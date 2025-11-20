#ifndef _RING_BUFFER_HPP
#define _RING_BUFFER_HPP

#include <cstdint>

template<typename T>
class RingBuffer
{
public:
	RingBuffer(uint64_t size)
	{
		_buffer = new T[size];
		_size = size;
		_begin = 0;
		_end = 0;
	}

	~RingBuffer()
	{
		delete[] _buffer;
	}

	bool IsEmpty()
	{
		return _begin == _end;
	}

	void Insert(const T& item)
	{
		_buffer[_end] = item;

		if (_end == _size - 1) {
			_end = 0;
		} else {
			++_end;
		}
	}

	T Get()
	{
		uint64_t pos = _begin;

		if (_begin == _size - 1) {
			_begin = 0;
		} else {
			++_begin;
		}

		return _buffer[pos];
	}

private:
	T *_buffer;
	uint64_t _size;
	volatile uint64_t _begin;
	volatile uint64_t _end;
};

#endif
