#ifndef _GATE_PROTOCOL_HPP
#define _GATE_PROTOCOL_HPP

#include "../Crypto/Crypto.hpp"

class GateProtocol
{
public:
	GateProtocol(
		Crypto::X25519::EncryptedStream *outES,
		Crypto::X25519::EncryptedStream *inES);
	~GateProtocol();

	void SetInputSizeLimit(uint64_t limit);

	bool ProcessRead(CowBuffer<uint8_t> buffer);
	bool HasInputBuffer();
	CowBuffer<uint8_t> GetInputBuffer();

	void AddBufferForOutput(CowBuffer<uint8_t> buffer);
	bool HasOutput();
	CowBuffer<uint8_t> GetOutputBuffer();

private:
	Crypto::X25519::EncryptedStream *_outES;
	Crypto::X25519::EncryptedStream *_inES;

	// Output processing.
	struct OutQueueNode
	{
		OutQueueNode *Next;
		CowBuffer<uint8_t> Data;
	};

	OutQueueNode *_firstOutNode;
	OutQueueNode *_lastOutNode;

	CowBuffer<uint8_t> _outputBuffer;
	uint64_t _outputBufferOffset;

	// Input processing.
	uint64_t _inputSizeLimit;
	bool _waitingForSize;
	CowBuffer<uint8_t> _inputBuffer;
	uint64_t _inputBufferOffset;
};

#endif
