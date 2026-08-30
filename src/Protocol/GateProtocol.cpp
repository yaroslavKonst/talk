#include "GateProtocol.hpp"

#include "CommonParserConstants.hpp"
#include "../Common/Endianness.hpp"
#include "../Common/Exception.hpp"

GateProtocol::GateProtocol(
	Crypto::X25519::EncryptedStream *outES,
	Crypto::X25519::EncryptedStream *inES)
{
	_outES = outES;
	_inES = inES;

	_firstOutNode = nullptr;
	_lastOutNode = nullptr;

	_outputBufferOffset = 0;

	_inputSizeLimit = CommonParserConstants::SmallDatagramSize;
	_inputBufferOffset = 0;
}

GateProtocol::~GateProtocol()
{
	while (_firstOutNode) {
		OutQueueNode *tmp = _firstOutNode;
		_firstOutNode = _firstOutNode->Next;
		delete tmp;
	}

	_lastOutNode = nullptr;
}

void GateProtocol::SetInputSizeLimit(uint64_t limit)
{
	_inputSizeLimit = limit;
}

bool GateProtocol::ProcessRead(CowBuffer<uint8_t> buffer)
{
	CowBuffer<uint8_t> decryptedBuffer =
		Crypto::X25519::Decrypt(buffer, *_inES);

	if (!decryptedBuffer.Size()) {
		return false;
	}

	if (!_inputBuffer.Size()) {
		if (decryptedBuffer.Size() <= sizeof(uint64_t)) {
			return false;
		}

		uint64_t size = SetProtoEndian(
			*decryptedBuffer.SwitchType<uint64_t>());

		if (!size || size > _inputSizeLimit) {
			return false;
		}

		_inputBuffer = CowBuffer<uint8_t>(size);
		_inputBufferOffset = 0;

		decryptedBuffer = decryptedBuffer.Slice(
			sizeof(size),
			decryptedBuffer.Size() - sizeof(size));
	}

	if (_inputBuffer.Size() - _inputBufferOffset < decryptedBuffer.Size()) {
		return false;
	}

	memcpy(
		_inputBuffer.Pointer(_inputBufferOffset),
		decryptedBuffer.Pointer(),
		decryptedBuffer.Size());

	_inputBufferOffset += decryptedBuffer.Size();

	return true;
}

bool GateProtocol::HasInputBuffer()
{
	return _inputBuffer.Size() && _inputBufferOffset >= _inputBuffer.Size();
}

CowBuffer<uint8_t> GateProtocol::GetInputBuffer()
{
	CowBuffer<uint8_t> buffer = _inputBuffer;
	_inputBuffer = CowBuffer<uint8_t>();
	_inputBufferOffset = 0;
	return buffer;
}

void GateProtocol::AddBufferForOutput(CowBuffer<uint8_t> buffer)
{
	OutQueueNode *node = new OutQueueNode;
	node->Next = nullptr;
	node->Data = buffer;

	if (!_firstOutNode) {
		_firstOutNode = node;
		_lastOutNode = node;
	} else {
		_lastOutNode->Next = node;
		_lastOutNode = node;
	}
}

bool GateProtocol::HasOutput()
{
	return _firstOutNode || _outputBuffer.Size();
}

CowBuffer<uint8_t> GateProtocol::GetOutputBuffer()
{
	CowBuffer<uint8_t> prefix;

	if (!_outputBuffer.Size()) {
		if (!_firstOutNode) {
			THROW("Output queue is empty.");
		}

		_outputBuffer = _firstOutNode->Data;
		_outputBufferOffset = 0;

		OutQueueNode *tmp = _firstOutNode;
		_firstOutNode = _firstOutNode->Next;
		delete tmp;

		if (!_firstOutNode) {
			_lastOutNode = nullptr;
		}

		prefix = CowBuffer<uint8_t>(sizeof(uint64_t));
		*prefix.SwitchType<uint64_t>() =
			SetProtoEndian(_outputBuffer.Size());
	}

	uint64_t size = _outputBuffer.Size() - _outputBufferOffset;

	if (size > 3072) {
		size = 2048;
	}

	CowBuffer<uint8_t> result = prefix.Concat(
		_outputBuffer.Slice(_outputBufferOffset, size));

	_outputBufferOffset += size;

	if (_outputBufferOffset >= _outputBuffer.Size()) {
		_outputBuffer = CowBuffer<uint8_t>();
		_outputBufferOffset = 0;
	}

	return Crypto::X25519::Encrypt(result, *_outES);
}
