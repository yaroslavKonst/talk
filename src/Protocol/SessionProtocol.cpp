#include "SessionProtocol.hpp"

#include "../Common/Exception.hpp"

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

// Multiplexer.
Multiplexer::Multiplexer(int channelCount)
{
	_channelCount = channelCount;

	_inputQueues = new BufferQueue[_channelCount];
	_inProgressBuffers = new CowBuffer<uint8_t>[_channelCount];
	_bytesToWrite = new uint64_t[_channelCount];
}

Multiplexer::~Multiplexer()
{
	delete[] _bytesToWrite;
	delete[] _inProgressBuffers;
	delete[] _inputQueues;
}

void Multiplexer::AddBuffer(CowBuffer<uint8_t> buffer, int channel)
{
	if (channel >= _channelCount) {
		THROW("Channel index is too big.");
	}

	_inputQueues[channel].Put(buffer);
}

bool Multiplexer::HasData()
{
	for (int i = 0; i < _channelCount; i++) {
		if (_bytesToWrite[i] || !_inputQueues[i].IsEmpty()) {
			return true;
		}
	}

	return false;
}

CowBuffer<uint8_t> Multiplexer::GetData()
{
	for (int i = 0; i < _channelCount; i++) {
		if (!_bytesToWrite[i] && _inputQueues[i].IsEmpty()) {
			continue;
		}

		CowBuffer<uint8_t> channel(1);
		channel[0] = i;

		if (!_bytesToWrite[i]) {
			_inProgressBuffers[i] = _inputQueues[i].Get();
			_bytesToWrite[i] = _inProgressBuffers[i].Size();

			CowBuffer<uint8_t> size(sizeof(uint64_t));
			*size.SwitchType<uint64_t>() =
				_inProgressBuffers[i].Size();

			channel = channel.Concat(size);
		}

		uint64_t offset =
			_inProgressBuffers[i].Size() - _bytesToWrite[i];
		uint64_t length;

		if (_bytesToWrite[i] > 1024 * 3) {
			length = 2048;
		} else {
			length = _bytesToWrite[i];
		}

		_bytesToWrite[i] -= length;
		CowBuffer<uint8_t> data =
			_inProgressBuffers[i].Slice(offset, length);

		if (!_bytesToWrite[i]) {
			_inProgressBuffers[i] = CowBuffer<uint8_t>();
		}

		return channel.Concat(data);
	}

	THROW("Multiplexer has no data.");
}

// Demultiplexer.
Demultiplexer::Demultiplexer(int channelCount)
{
	_channelCount = channelCount;

	_inProgressBuffers = new CowBuffer<uint8_t>[_channelCount];
	_bytesToRead = new uint64_t[_channelCount];

	_inputSizeLimit = 1024 * 4;
}

Demultiplexer::~Demultiplexer()
{
	delete[] _bytesToRead;
	delete[] _inProgressBuffers;
}

void Demultiplexer::SetInputSizeLimit(uint64_t size)
{
	_inputSizeLimit = size;
}

bool Demultiplexer::AddData(CowBuffer<uint8_t> data)
{
	if (data.Size() == 0) {
		return false;
	}

	int channel = data[0];

	if (channel >= _channelCount) {
		return false;
	}

	data = data.Slice(1, data.Size() - 1);

	if (!_bytesToRead[channel]) {
		if (data.Size() < sizeof(uint64_t)) {
			return false;
		}

		uint64_t size = *data.SwitchType<uint64_t>(1);

		if (size > _inputSizeLimit) {
			return false;
		}

		_bytesToRead[channel] = size;
		_inProgressBuffers[channel] = CowBuffer<uint8_t>(size);
		data = data.Slice(sizeof(size), data.Size() - sizeof(size));
	}

	if (data.Size() == 0) {
		return false;
	}

	if (data.Size() > _bytesToRead[channel]) {
		return false;
	}

	memcpy(
		_inProgressBuffers[channel].Pointer(
			_inProgressBuffers[channel].Size() -
			_bytesToRead[channel]),
		data.Pointer(),
		data.Size());

	_bytesToRead[channel] -= data.Size();

	return true;
}

bool Demultiplexer::HasBuffer()
{
	for (int i = 0; i < _channelCount; i++) {
		if (!_bytesToRead[i] && _inProgressBuffers[i].Size()) {
			return true;
		}
	}

	return false;
}

CowBuffer<uint8_t> Demultiplexer::GetBuffer()
{
	for (int i = 0; i < _channelCount; i++) {
		if (!_bytesToRead[i] && _inProgressBuffers[i].Size()) {
			CowBuffer<uint8_t> buffer = _inProgressBuffers[i];
			_inProgressBuffers[i] = CowBuffer<uint8_t>();
			return buffer;
		}
	}

	THROW("Demultiplexer has no data for reading.");
}

// Session.
SessionProtocol::SessionProtocol(
	int fd,
	EncryptedStream *outES,
	EncryptedStream *inES,
	uint8_t outScramblerInit,
	uint8_t inScramblerInit)
{
	_fd = fd;
	_outES = outES;
	_inES = inES;
	_outScramblerInit = outScramblerInit;
	_inScramblerInit = inScramblerInit;

	_reader = new StreamReader(_fd, sizeof(uint64_t));
	_writer = nullptr;

	_mux = new Multiplexer(StreamCount);
	_demux = new Demultiplexer(StreamCount);

	_waitingSize = true;
}

SessionProtocol::~SessionProtocol()
{
	delete _mux;
	_mux = nullptr;
	delete _demux;
	_demux = nullptr;

	if (_reader) {
		delete _reader;
		_reader = nullptr;
	}

	if (_writer) {
		delete _writer;
		_writer = nullptr;
	}
}

void SessionProtocol::SetInputSizeLimit(uint64_t size)
{
	_demux->SetInputSizeLimit(size);
}

bool SessionProtocol::Read()
{
	if (!_reader) {
		THROW("Reader is not initialized.");
	}

	bool readSuccess = _reader->Read();

	if (!readSuccess) {
		return false;
	}

	bool readEnded = _reader->ReadingEnd();

	if (!readEnded) {
		return true;
	}

	CowBuffer<uint8_t> buffer = _reader->GetBuffer();
	delete _reader;
	_reader = nullptr;

	_inScramblerInit = ApplyScrambler(
		buffer.Pointer(),
		buffer.Size(),
		_inScramblerInit);

	if (_waitingSize) {
		if (buffer.Size() != sizeof(uint64_t)) {
			THROW("Size was expected.");
		}

		uint64_t size = *buffer.SwitchType<uint8_t>();

		if (size > 4 * 1024 || !size) {
			return false;
		}

		_waitingSize = false;

		_reader = new StreamReader(_fd, size);
		return true;
	}

	_waitingSize = true;
	_reader = new StreamReader(_fd, sizeof(uint64_t));

	buffer = Decrypt(buffer, *_inES);

	if (!buffer.Size()) {
		return false;
	}

	return _demux->AddData(buffer);
}

bool SessionProtocol::Write()
{
	if (_writer) {
		bool writeSuccess = _writer->Write();

		if (!writeSuccess) {
			return false;
		}

		bool writingEnded = _writer->WritingEnd();

		if (!writingEnded) {
			return true;
		}

		delete _writer;
		_writer = nullptr;
	}

	if (!_mux->HasData()) {
		return true;
	}

	CowBuffer<uint8_t> data = _mux->GetData();

	data = Encrypt(data, *_outES);

	CowBuffer<uint8_t> sizeBuffer(sizeof(uint64_t));
	*sizeBuffer.SwitchType<uint64_t>() = data.Size();
	data = sizeBuffer.Concat(data);

	_outScramblerInit = ApplyScrambler(
		data.Pointer(),
		data.Size(),
		_outScramblerInit);

	_writer = new StreamWriter(_fd, data);
	return true;
}

bool SessionProtocol::RequestWrite()
{
	return _writer || _mux->HasData();
}

bool SessionProtocol::CanReceive()
{
	return _demux->HasBuffer();
}

CowBuffer<uint8_t> SessionProtocol::Receive()
{
	return _demux->GetBuffer();
}

void SessionProtocol::Send(CowBuffer<uint8_t> data, int stream)
{
	_mux->AddBuffer(data, stream);
}
