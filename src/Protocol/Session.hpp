#ifndef _SESSION_HPP
#define _SESSION_HPP

#include "../Common/CowBuffer.hpp"

class BufferQueue
{
public:
	BufferQueue();
	~BufferQueue();

	bool IsEmpty();

	void Put(const CowBuffer<uint8_t> buffer);
	CowBuffer<uint8_t> Get();

	void Clear();

private:
	struct Sequence
	{
		Sequence *Next;
		CowBuffer<uint8_t> Data;
	};

	Sequence *_first;
	Sequence *_last;
};

class StreamReader
{
public:
	StreamReader();

	bool Finalized();

	bool HasData();
	CowBuffer<uint8_t> GetData();

	bool Process(int sockFd, uint64_t sizeLimit);

	void Reset();

private:
	CowBuffer<uint8_t> _data;
	uint64_t _expectedData;

	uint64_t _expectedSlice;

	BufferQueue _queue;
};

class StreamWriter
{
public:
	StreamWriter();

	bool Finalized();

	bool CanWrite();
	void AddData(const CowBuffer<uint8_t> data);

	bool Process(int sockFd, uint8_t stream);

	void Reset();

private:
	CowBuffer<uint8_t> _data;
	uint64_t _remainingData;

	uint64_t _remainingSlice;

	BufferQueue _queue;
};

struct Session
{
	enum
	{
		StreamCount = 3
	};

	Session();
	virtual ~Session();

	Session *Next;

	int64_t Time;
	int Socket;

	uint64_t InputSizeLimit;
	bool RestrictStreams;

	bool Read();
	bool Write();

	StreamReader InputStreams[StreamCount];
	StreamWriter OutputStreams[StreamCount];

	bool CanWrite();
	bool CanReceive();

	CowBuffer<uint8_t> Receive(int *stream = nullptr);
	void Send(CowBuffer<uint8_t> data, int stream);

	virtual bool Process();
	virtual bool TimePassed();

	void Close();

	bool Closed()
	{
		return Socket == -1;
	}
};

#endif
