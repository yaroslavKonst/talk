#include "Session.hpp"

#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>

#include "../Common/UnixTime.hpp"
#include "../Common/Exception.hpp"

static int64_t Read(int fd, void *buffer, int64_t size)
{
	for (;;) {
		int64_t res = read(fd, buffer, size);

		if (res == -1) {
			if (errno == EINTR) {
				continue;
			}
		}

		return res;
	}
}

static int64_t Write(int fd, const void *buffer, int64_t size)
{
	for (;;) {
		int64_t res = write(fd, buffer, size);

		if (res == -1) {
			if (errno == EINTR) {
				continue;
			}
		}

		return res;
	}
}

static int64_t LoopRead(int fd, void *buffer, int64_t size)
{
	int64_t readBytes = 0;

	while (readBytes < size) {
		int64_t rb = Read(
			fd,
			(char*)buffer + readBytes,
			size - readBytes);

		if (rb <= 0) {
			return readBytes;
		}

		readBytes += rb;
	}

	return readBytes;
}

static int64_t LoopWrite(int fd, const void *buffer, int64_t size)
{
	int64_t writtenBytes = 0;

	while (writtenBytes < size) {
		int64_t wb = Write(
			fd,
			(const char*)buffer + writtenBytes,
			size - writtenBytes);

		if (wb <= 0) {
			return writtenBytes;
		}

		writtenBytes += wb;
	}

	return writtenBytes;
}

// BufferQueue.
BufferQueue::BufferQueue()
{
	_first = nullptr;
	_last = nullptr;
}

BufferQueue::~BufferQueue()
{
	Clear();
}

bool BufferQueue::IsEmpty()
{
	return !_first;
}

void BufferQueue::Put(const CowBuffer<uint8_t> buffer)
{
	Sequence *seq = new Sequence;
	seq->Next = nullptr;
	seq->Data = buffer;

	if (!_first) {
		_first = seq;
		_last = seq;
	} else {
		_last->Next = seq;
		_last = seq;
	}
}

CowBuffer<uint8_t> BufferQueue::Get()
{
	if (!_first) {
		THROW("Buffer queue is empty.");
	}

	Sequence *tmp = _first;
	_first = _first->Next;

	if (!_first) {
		_last = nullptr;
	}

	CowBuffer<uint8_t> result = tmp->Data;
	delete tmp;
	return result;
}

void BufferQueue::Clear()
{
	while (_first) {
		Sequence *tmp = _first;
		_first = _first->Next;
		delete tmp;
	}

	_last = nullptr;
}

// StreamReader.
StreamReader::StreamReader()
{
	_expectedData = 0;
	_expectedSlice = 0;
	_expectedInt = 0;
}

bool StreamReader::Finalized()
{
	return !_expectedSlice && !_expectedInt;
}

bool StreamReader::HasData()
{
	return !_queue.IsEmpty();
}

CowBuffer<uint8_t> StreamReader::GetData()
{
	return _queue.Get();
}

bool StreamReader::Process(int sockFd, uint64_t sizeLimit)
{
	if (!_expectedData) {
		return ReadDataSize(sockFd, sizeLimit);
	}

	if (!_expectedSlice) {
		return ReadSliceSize(sockFd);
	}

	return ReadSlice(sockFd);
}

void StreamReader::Reset()
{
	_data = CowBuffer<uint8_t>();
	_expectedData = 0;
	_expectedSlice = 0;
	_intBuffer = CowBuffer<uint8_t>();
	_expectedInt = 0;
	_queue.Clear();
}

bool StreamReader::ReadDataSize(int sockFd, uint64_t sizeLimit)
{
	uint64_t dataSize;

	if (!_expectedInt) {
		_intBuffer = CowBuffer<uint8_t>(sizeof(dataSize));
		_expectedInt = sizeof(dataSize);
		return true;
	}

	int rb = LoopRead(
		sockFd,
		_intBuffer.Pointer(_intBuffer.Size() - _expectedInt),
		_expectedInt);

	if (!rb) {
		return false;
	}

	_expectedInt -= rb;

	if (_expectedInt) {
		return true;
	}

	dataSize = *_intBuffer.SwitchType<uint64_t>();
	_intBuffer = CowBuffer<uint8_t>();

	if (!dataSize || dataSize > sizeLimit) {
		return false;
	}

	_expectedData = dataSize;
	_data = CowBuffer<uint8_t>(dataSize);

	return true;
}

bool StreamReader::ReadSliceSize(int sockFd)
{
	uint32_t sliceSize;

	if (!_expectedInt) {
		_intBuffer = CowBuffer<uint8_t>(sizeof(sliceSize));
		_expectedInt = sizeof(sliceSize);
		return true;
	}

	int rb = LoopRead(
		sockFd,
		_intBuffer.Pointer(_intBuffer.Size() - _expectedInt),
		_expectedInt);

	if (!rb) {
		return false;
	}

	_expectedInt -= rb;

	if (_expectedInt) {
		return true;
	}

	sliceSize = *_intBuffer.SwitchType<uint32_t>();
	_intBuffer = CowBuffer<uint8_t>();

	if (!sliceSize || sliceSize > 2048) {
		return false;
	}

	_expectedSlice = sliceSize;

	if (_expectedSlice > _expectedData) {
		return false;
	}

	return true;
}

bool StreamReader::ReadSlice(int sockFd)
{
	int rb = Read(
		sockFd,
		_data.Pointer(_data.Size() - _expectedData),
		_expectedSlice);

	if (rb <= 0) {
		return false;
	}

	_expectedSlice -= rb;
	_expectedData -= rb;

	if (!_expectedData && _expectedSlice) {
		return false;
	}

	if (!_expectedData) {
		_queue.Put(_data);
		_data = CowBuffer<uint8_t>();
	}

	return true;
}

// StreamWriter.
StreamWriter::StreamWriter()
{
	_remainingData = 0;
	_remainingSlice = 0;
	_remainingInt = 0;
}

bool StreamWriter::Finalized()
{
	return !_remainingSlice && !_remainingInt;
}

bool StreamWriter::CanWrite()
{
	return _remainingData || !_queue.IsEmpty() || _remainingInt;
}

void StreamWriter::AddData(const CowBuffer<uint8_t> data)
{
	_queue.Put(data);
}

bool StreamWriter::Process(int sockFd, uint8_t stream)
{
	if (_remainingInt) {
		return WriteInt(sockFd);
	}

	if (!_remainingData) {
		return WriteDataSize(sockFd, stream);
	}

	if (!_remainingSlice) {
		return WriteSliceSize(sockFd, stream);
	}

	return WriteSlice(sockFd);
}

void StreamWriter::Reset()
{
	_data = CowBuffer<uint8_t>();
	_remainingData = 0;
	_remainingSlice = 0;
	_intBuffer = CowBuffer<uint8_t>();
	_remainingInt = 0;
	_queue.Clear();
}

bool StreamWriter::WriteInt(int sockFd)
{
	int wb = LoopWrite(
		sockFd,
		_intBuffer.Pointer(_intBuffer.Size() - _remainingInt),
		_remainingInt);

	if (!wb) {
		return false;
	}

	_remainingInt -= wb;

	if (!_remainingInt) {
		_intBuffer = CowBuffer<uint8_t>();
	}

	return true;
}

bool StreamWriter::WriteDataSize(int sockFd, uint8_t stream)
{
	if (_queue.IsEmpty()) {
		return true;
	}

	_data = _queue.Get();
	_remainingData = _data.Size();

	int wb = LoopWrite(sockFd, &stream, 1);

	if (wb != 1) {
		return false;
	}

	_intBuffer = CowBuffer<uint8_t>(sizeof(uint64_t));
	*_intBuffer.SwitchType<uint64_t>() = _remainingData;
	_remainingInt = _intBuffer.Size();

	return true;
}

bool StreamWriter::WriteSliceSize(int sockFd, uint8_t stream)
{
	_remainingSlice = _remainingData;

	if (_remainingSlice > 2048) {
		_remainingSlice = 2048;
	}

	int wb = LoopWrite(sockFd, &stream, 1);

	if (wb != 1) {
		return false;
	}

	_intBuffer = CowBuffer<uint8_t>(sizeof(uint32_t));
	*_intBuffer.SwitchType<uint32_t>() = _remainingSlice;
	_remainingInt = _intBuffer.Size();

	return true;
}

bool StreamWriter::WriteSlice(int sockFd)
{
	int wb = Write(
		sockFd,
		_data.Pointer(_data.Size() - _remainingData),
		_remainingSlice);

	if (wb <= 0) {
		return false;
	}

	_remainingData -= wb;
	_remainingSlice -= wb;

	if (!_remainingData) {
		_remainingSlice = 0;
		_data = CowBuffer<uint8_t>();
	}

	return true;
}

// Session.
Session::Session()
{
	Next = nullptr;

	Time = GetUnixTime();
	Socket = -1;

	InputSizeLimit = 1024;
	RestrictStreams = true;
}

Session::~Session()
{
	Close();
}

bool Session::Read()
{
	if (Closed()) {
		return false;
	}

	Time = GetUnixTime();

	int maxStream = RestrictStreams ? 0 : StreamCount - 1;

	for (int i = 0; i <= maxStream; i++) {
		if (!InputStreams[i].Finalized()) {
			return InputStreams[i].Process(Socket, InputSizeLimit);
		}
	}

	uint8_t stream;

	int rb = LoopRead(Socket, &stream, 1);

	if (rb != 1) {
		return false;
	}

	if (stream > maxStream) {
		return false;
	}

	return InputStreams[stream].Process(Socket, InputSizeLimit);
}

bool Session::Write()
{
	if (Closed()) {
		return false;
	}

	Time = GetUnixTime();

	int maxStream = RestrictStreams ? 0 : StreamCount - 1;

	for (int i = 0; i <= maxStream; i++) {
		if (!OutputStreams[i].Finalized()) {
			return OutputStreams[i].Process(Socket, i);
		}
	}

	for (int i = 0; i <= maxStream; i++) {
		if (!OutputStreams[i].CanWrite()) {
			continue;
		}

		return OutputStreams[i].Process(Socket, i);
	}

	return true;
}

bool Session::CanWrite()
{
	int maxStream = RestrictStreams ? 0 : StreamCount - 1;

	for (int i = 0; i <= maxStream; i++) {
		if (OutputStreams[i].CanWrite()) {
			return true;
		}
	}

	return false;
}

bool Session::CanReceive()
{
	for (int i = 0; i < StreamCount; i++) {
		if (InputStreams[i].HasData()) {
			return true;
		}
	}

	return false;
}

CowBuffer<uint8_t> Session::Receive(int *stream)
{
	for (int i = 0; i < StreamCount; i++) {
		if (InputStreams[i].HasData()) {
			if (stream) {
				*stream = i;
			}

			return InputStreams[i].GetData();
		}
	}

	THROW("No data can be received.");
}

void Session::Send(CowBuffer<uint8_t> data, int stream)
{
	if (stream >= StreamCount) {
		THROW("Invalid stream index.");
	}

	if (!data.Size()) {
		THROW("Transmitted data cannot be empty.");
	}

	OutputStreams[stream].AddData(data);
}

bool Session::Process()
{
	return false;
}

bool Session::TimePassed()
{
	return false;
}

void Session::Close()
{
	if (Socket != -1) {
		shutdown(Socket, SHUT_RDWR);

		bool intr;

		do {
			intr = false;
			int ret = close(Socket);

			if (ret == -1 && errno == EINTR) {
				intr = true;
			}
		} while (intr);

		Socket = -1;
	}

	for (int i = 0; i < StreamCount; i++) {
		InputStreams[i].Reset();
		OutputStreams[i].Reset();
	}
}
