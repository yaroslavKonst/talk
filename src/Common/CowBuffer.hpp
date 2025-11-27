#ifndef _COW_BUFFER_HPP
#define _COW_BUFFER_HPP

#include <cstdint>
#include <cstring>

template <typename T>
class CowBuffer
{
public:
	CowBuffer()
	{
		_data = new Data;
		_data->RefCount = 1;
		_data->Data = nullptr;

		_size = 0;
		_offset = 0;
	}

	CowBuffer(uint64_t size)
	{
		_data = new Data;
		_data->RefCount = 1;

		_size = size;
		_offset = 0;

		if (size) {
			_data->Data = new T[size];
		} else {
			_data->Data = nullptr;
		}
	}

	CowBuffer(const CowBuffer &cb)
	{
		_data = cb._data;
		_size = cb._size;
		_offset = cb._offset;
		IncRef();
	}

	~CowBuffer()
	{
		DecRef();
	}

	CowBuffer &operator=(const CowBuffer &cb)
	{
		if (_data != cb._data) {
			DecRef();
			_data = cb._data;
			IncRef();
		}

		_size = cb._size;
		_offset = cb._offset;

		return *this;
	}

	void Wipe()
	{
		if (_data->RefCount > 1 || !_size) {
			Resize(0);
			return;
		}

		memset(_data->Data + _offset, 0, _size * sizeof(T));
		Resize(0);
	}

	T operator[](uint64_t index) const
	{
		return _data->Data[index + _offset];
	}

	T &operator[](uint64_t index)
	{
		MakeExclusive();
		return _data->Data[index + _offset];
	}

	T *Pointer(int offset = 0)
	{
		MakeExclusive();
		return _data->Data + _offset + offset;
	}

	const T *Pointer(int offset = 0) const
	{
		return _data->Data + _offset + offset;
	}

	uint64_t Size() const
	{
		return _size;
	}

	void Resize(uint64_t size)
	{
		if (_size >= size) {
			_size = size;
			return;
		}

		MakeExclusive();

		T *newBuffer = nullptr;

		if (size) {
			newBuffer = new T[size];
		}

		if (_size && size) {
			CopyData(newBuffer, _data->Data + _offset, _size);
		}

		if (_size || _offset) {
			delete[] _data->Data;
		}

		_size = size;
		_offset = 0;
		_data->Data = newBuffer;
	}

	CowBuffer Slice(uint64_t start, uint64_t length) const
	{
		CowBuffer result = *this;
		result._offset = _offset + start;
		result._size = length;
		return result;
	}

	CowBuffer Concat(const CowBuffer buffer) const
	{
		uint64_t size = _size + buffer._size;

		CowBuffer result(size);

		if (_size) {
			CopyData(result.Pointer(), Pointer(), _size);
		}

		if (buffer.Size()) {
			CopyData(
				result.Pointer() + _size,
				buffer.Pointer(),
				buffer.Size());
		}

		return result;
	}

	template <typename S>
	const S *SwitchType(int offset = 0) const
	{
		return (S*)(Pointer() + offset);
	}

	template <typename S>
	S *SwitchType(int offset = 0)
	{
		return (S*)(Pointer() + offset);
	}

private:
	static void CopyData(T *dest, const T *src, uint64_t size)
	{
		for (uint64_t i = 0; i < size; i++) {
			dest[i] = src[i];
		}
	}

	struct Data
	{
		int RefCount;
		T *Data;
	};

	Data *_data;
	uint64_t _offset;
	uint64_t _size;

	void IncRef()
	{
		_data->RefCount += 1;
	}

	void DecRef()
	{
		_data->RefCount -= 1;

		if (_data->RefCount <= 0) {
			if (_size || _offset) {
				delete[] _data->Data;
				_data->Data = nullptr;
			}

			delete _data;
			_data = nullptr;
		}
	}

	void MakeExclusive()
	{
		if (_data->RefCount == 1) {
			return;
		}

		Data *data = new Data;
		data->RefCount = 1;

		if (_size) {
			data->Data = new T[_size];
			CopyData(data->Data, _data->Data + _offset, _size);
		} else {
			data->Data = nullptr;
		}

		_offset = 0;

		DecRef();
		_data = data;
	}
};

#endif
