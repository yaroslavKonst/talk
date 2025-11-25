#include "Session.hpp"

#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>

#include "../Common/UnixTime.hpp"

Session::Session()
{
	Next = nullptr;

	Time = GetUnixTime();
	Socket = -1;

	InputSizeLimit = 1024;

	Input = nullptr;
	ExpectedInput = 0;

	Output = nullptr;
	RequiredOutput = 0;

	InputSequence = nullptr;
	InputSequenceLast = nullptr;

	OutputSequence = nullptr;
	OutputSequenceLast = nullptr;
}

Session::~Session()
{
	while (InputSequence) {
		Sequence *elem = InputSequence;
		InputSequence = InputSequence->Next;
		delete elem;
	}

	while (OutputSequence) {
		Sequence *elem = OutputSequence;
		OutputSequence = OutputSequence->Next;
		delete elem;
	}

	if (Input) {
		delete Input;
	}

	if (Output) {
		delete Output;
	}

	Close();
}

void Session::SetInputSizeLimit(uint64_t limit)
{
	InputSizeLimit = limit;
}

bool Session::Read()
{
	if (Closed()) {
		return false;
	}

	Time = GetUnixTime();

	if (!Input) {
		uint64_t size = 0;

		int res = read(Socket, &size, sizeof(size));

		if (res == -1) {
			if (errno == EINTR) {
				return true;
			}

			return false;
		}

		if (res != sizeof(size)) {
			return false;
		}

		if (size > InputSizeLimit) {
			return false;
		}

		if (!size) {
			return false;
		}

		ExpectedInput = size;
		Input = new CowBuffer<uint8_t>(size);
		return true;
	}

	int64_t readBytes = read(
		Socket,
		Input->Pointer() + Input->Size() - ExpectedInput,
		ExpectedInput);

	if (readBytes == -1) {
		if (errno == EINTR) {
			return true;
		}

		return false;
	}

	if (readBytes <= 0) {
		return false;
	}

	ExpectedInput -= readBytes;

	if (ExpectedInput == 0) {
		Sequence *elem = new Sequence;
		elem->Next = nullptr;
		elem->Data = *Input;
		delete Input;
		Input = nullptr;

		if (InputSequence) {
			InputSequenceLast->Next = elem;
			InputSequenceLast = elem;
		} else {
			InputSequence = elem;
			InputSequenceLast = elem;
		}
	}

	return true;
}

bool Session::Write()
{
	if (Closed()) {
		return false;
	}

	Time = GetUnixTime();

	if (!Output && !OutputSequence) {
		return true;
	}

	if (!Output) {
		Output = new CowBuffer<uint8_t>();

		Sequence *elem = OutputSequence;
		*Output = elem->Data;

		OutputSequence = elem->Next;

		if (!OutputSequence) {
			OutputSequenceLast = nullptr;
		}

		delete elem;
	}

	if (Output && !RequiredOutput)
	{
		uint64_t size = Output->Size();

		int res = write(Socket, &size, sizeof(size));

		if (res == -1) {
			if (errno == EINTR) {
				return true;
			}

			return false;
		}

		if (res != sizeof(size)) {
			return false;
		}

		RequiredOutput = size;
	}

	uint64_t limit = 1024 * 16;

	if (RequiredOutput < limit) {
		limit = RequiredOutput;
	}

	int64_t writtenBytes = write(
		Socket,
		Output->Pointer() + Output->Size() - RequiredOutput,
		limit);

	if (writtenBytes == -1) {
		if (errno == EINTR) {
			return true;
		}

		return false;
	}

	if (writtenBytes <= 0) {
		return false;
	}

	RequiredOutput -= writtenBytes;

	if (RequiredOutput == 0)
	{
		delete Output;
		Output = nullptr;
	}

	return true;
}

CowBuffer<uint8_t> Session::Receive()
{
	if (!InputSequence) {
		return CowBuffer<uint8_t>();
	}

	Sequence *elem = InputSequence;

	CowBuffer<uint8_t> data = elem->Data;

	InputSequence = elem->Next;

	if (!InputSequence) {
		InputSequenceLast = nullptr;
	}

	delete elem;

	return data;
}

void Session::Send(CowBuffer<uint8_t> data)
{
	Sequence *elem = new Sequence;
	elem->Next = nullptr;
	elem->Data = data;

	if (OutputSequence) {
		OutputSequenceLast->Next = elem;
		OutputSequenceLast = elem;
	} else {
		OutputSequence = elem;
		OutputSequenceLast = elem;
	}
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
}
