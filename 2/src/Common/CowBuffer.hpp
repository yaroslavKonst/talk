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
		_data->Size = 0;
		_data = nullptr;
	}

	CowBuffer(uint64_t size)
	{
		_data = new Data;
		_data->RefCount = 1;
		_data->Size = size;

		if (size) {
			_data->Data = new T[size];
		} else {
			_data = nullptr;
		}
	}

	CowBuffer(const CowBuffer &cb)
	{
		_data = cb._data;
		IncRef();
	}

	~CowBuffer()
	{
		DecRef();
	}

	CowBuffer &operator=(const CowBuffer &cb)
	{
		DecRef();
		_data = cb._data;
		IncRef();

		return *this;
	}

	T operator[](uint64_t index) const
	{
		return _data->Data[index];
	}

	T &operator[](uint64_t index)
	{
		MakeExclusive();
		return _data->Data[index];
	}

	T *Pointer()
	{
		MakeExclusive();
		return _data->Data;
	}

	const T *Pointer() const
	{
		return _data->Data;
	}

	uint64_t Size() const
	{
		return _data->Size;
	}

	void Resize(uint64_t size)
	{
		MakeExclusive();

		uint64_t copySize = size < _data->Size ? size : _data->Size;

		T *newBuffer = nullptr;

		if (size) {
			newBuffer = new T[size];
		}

		if (copySize) {
			memcpy(newBuffer, _data->Data, copySize * sizeof(T));
		}

		if (_data->Size) {
			delete[] _data->Data;
		}

		_data->Size = size;
		_data->Data = newBuffer;
	}

	CowBuffer Slice(uint64_t start, uint64_t length) const
	{
		CowBuffer result(length);
		memcpy(result.Pointer(), Pointer() + start, sizeof(T) * length);
		return result;
	}

private:
	struct Data
	{
		int RefCount;
		uint64_t Size;
		T *Data;
	};

	Data *_data;

	void IncRef()
	{
		_data->RefCount += 1;
	}

	void DecRef()
	{
		_data->RefCount -= 1;

		if (_data->RefCount <= 0) {
			if (_data->Size) {
				delete[] _data->Data;
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
		data->Size = _data->Size;

		if (data->Size) {
			data->Data = new T[data->Size];
			memcpy(data->Data, _data->Data, data->Size * sizeof(T));
		} else {
			data->Data = nullptr;
		}

		DecRef();
		_data = data;
	}
};

#endif
