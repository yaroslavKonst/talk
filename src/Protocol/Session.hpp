#ifndef _SESSION_HPP
#define _SESSION_HPP

#include "../Common/CowBuffer.hpp"

struct Session
{
	struct Sequence
	{
		Sequence *Next;
		CowBuffer<uint8_t> Data;
	};

	Session();
	virtual ~Session();

	Session *Next;

	int64_t Time;
	int Socket;

	uint64_t ExpectedInput;
	CowBuffer<uint8_t> *Input;

	uint64_t RequiredOutput;
	CowBuffer<uint8_t> *Output;

	Sequence *InputSequence;
	Sequence *InputSequenceLast;

	Sequence *OutputSequence;
	Sequence *OutputSequenceLast;

	bool Read();
	bool Write();

	bool CanWrite()
	{
		return Output || OutputSequence;
	}

	bool CanReceive()
	{
		return InputSequence;
	}

	CowBuffer<uint8_t> Receive();
	void Send(CowBuffer<uint8_t> data);

	virtual bool Process();
	virtual bool TimePassed();
};

#endif
