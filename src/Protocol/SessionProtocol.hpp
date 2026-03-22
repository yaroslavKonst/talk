#ifndef _SESSION_PROTOCOL_HPP
#define _SESSION_PROTOCOL_HPP

#include "../Common/CowBuffer.hpp"
#include "../Common/StreamReader.hpp"
#include "../Common/StreamWriter.hpp"
#include "../Crypto/Crypto.hpp"

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

class Multiplexer
{
public:
	Multiplexer(int channelCount);
	~Multiplexer();

	void AddBuffer(CowBuffer<uint8_t> buffer, int channel);

	bool HasData();
	CowBuffer<uint8_t> GetData();

private:
	int _channelCount;
	BufferQueue *_inputQueues;

	CowBuffer<uint8_t> *_inProgressBuffers;
	uint64_t *_bytesToWrite;
};

class Demultiplexer
{
public:
	Demultiplexer(int channelCount);
	~Demultiplexer();

	void SetInputSizeLimit(uint64_t size);

	bool AddData(CowBuffer<uint8_t> data);

	bool HasBuffer();
	CowBuffer<uint8_t> GetBuffer();

private:
	int _channelCount;
	uint64_t _inputSizeLimit;

	CowBuffer<uint8_t> *_inProgressBuffers;
	uint64_t *_bytesToRead;
};

class SessionProtocol
{
public:
	SessionProtocol(
		int fd,
		EncryptedStream *outES,
		EncryptedStream *inES,
		uint8_t outScramblerInit,
		uint8_t inScramblerInit);
	~SessionProtocol();

	void SetInputSizeLimit(uint64_t size);

	bool Read();
	bool Write();

	bool RequestWrite();
	bool CanReceive();

	CowBuffer<uint8_t> Receive();
	void Send(CowBuffer<uint8_t> data, int stream);

private:
	enum
	{
		StreamCount = 2
	};

	int _fd;
	EncryptedStream *_outES;
	EncryptedStream *_inES;

	uint8_t _inScramblerInit;
	uint8_t _outScramblerInit;

	StreamReader *_reader;
	StreamWriter *_writer;

	Multiplexer *_mux;
	Demultiplexer *_demux;

	bool _waitingSize;
};

#endif
