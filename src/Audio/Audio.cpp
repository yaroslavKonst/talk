#include "Audio.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <cstdio>

#include "../Common/Exception.hpp"

#define RING_BUFFER_SIZE 1024 * 16

Audio::Audio() : _inBuffer(RING_BUFFER_SIZE), _outBuffer(RING_BUFFER_SIZE)
{
	_activeBuffers = nullptr;

	SetupPipes();
	SetupStream();
}

Audio::~Audio()
{
	CloseStream();

	while (!_inBuffer.IsEmpty()) {
		delete _inBuffer.Get();
	}

	while (!_outBuffer.IsEmpty()) {
		delete _outBuffer.Get();
	}

	while (_activeBuffers) {
		PlayState* tmp = _activeBuffers;
		_activeBuffers = _activeBuffers->Next;
		delete tmp;
	}

	ClosePipes();
}

void Audio::Submit(Sound *buffer)
{
	PlayState* playState = new PlayState(buffer);

	buffer->Finished = false;

	_inBuffer.Insert(playState);

	while (!_outBuffer.IsEmpty()) {
		PlayState *playState = _outBuffer.Get();
		delete playState;
	}
}

void Audio::WriteRaw(CowBuffer<int16_t> sound)
{
	uint32_t bytesToWrite = sound.Size() * sizeof(int16_t);
	uint32_t writtenBytes = 0;

	do {
		int res = write(
			_outputPipe[1],
			(const uint8_t*)sound.Pointer() + writtenBytes,
			bytesToWrite - writtenBytes);

		if (res > 0) {
			writtenBytes += res;
		} else {
			THROW("Failed to write raw sound.");
		}
	} while (writtenBytes < bytesToWrite);
}

CowBuffer<int16_t> Audio::ReadRaw()
{
	int bytesToRead = 1024;
	CowBuffer<int16_t> result(bytesToRead / sizeof(int16_t));

	int res = read(_inputPipe[0], result.Pointer(), bytesToRead);

	if (res <= 0) {
		THROW("Failed to read raw sound.");
	}

	if (res % 2) {
		while (read(
			_inputPipe[0],
			(uint8_t*)result.Pointer() + res,
			1) != 1) { }
		++res;
	}

	if (res == bytesToRead) {
		return result;
	}

	return result.Slice(0, res / sizeof(int16_t));
}

int Audio::GetSoundReadFileDescriptor()
{
	return _inputPipe[0];
}

void Audio::SetupStream()
{
	PaError err = Pa_Initialize();

	if (err != paNoError) {
		THROW(String("Failed to initialize portaudio: ") +
			Pa_GetErrorText(err));
	}

	err = Pa_OpenDefaultStream(
		&_stream,
		1, // mono input
		1, // mono output
		paInt16, // 16 bit integer sample type
		SampleRate,
		SampleCount, // frames per buffer
		AudioCallback, // callback function
		this);

	if (err != paNoError) {
		THROW(String("Failed to open stream: ") +
			Pa_GetErrorText(err));
	}

	err = Pa_StartStream(_stream);

	if (err != paNoError) {
		THROW(String("Failed to start stream: ") +
			Pa_GetErrorText(err));
	}
}

void Audio::CloseStream()
{
	PaError err = Pa_StopStream(_stream);

	if (err != paNoError) {
		printf((String("Failed to stop stream: ") +
			Pa_GetErrorText(err)).CStr());
	}

	err = Pa_CloseStream(_stream);

	if (err != paNoError) {
		printf((String("Failed to close stream: ") +
			Pa_GetErrorText(err)).CStr());
	}

	err = Pa_Terminate();

	if (err != paNoError) {
		printf((String("Failed to terminate portaudio: ") +
			Pa_GetErrorText(err)).CStr());
	}
}

void Audio::SetupPipes()
{
	int res = pipe(_inputPipe);

	if (res == -1) {
		THROW("Failed to create input pipe.");
	}

	res = pipe(_outputPipe);

	if (res == -1) {
		THROW("Failed to create output pipe.");
	}

	MakeNonblocking(_outputPipe[0]);
	MakeNonblocking(_inputPipe[1]);
}

void Audio::ClosePipes()
{
	close(_inputPipe[0]);
	close(_inputPipe[1]);
	close(_outputPipe[0]);
	close(_outputPipe[1]);
}

void Audio::MakeNonblocking(int fd)
{
	int flags = fcntl(fd, F_GETFL);
	flags |= O_NONBLOCK;
	int res = fcntl(fd, F_SETFL, flags);

	if (res == -1) {
		THROW("Failed to make file descriptor nonblocking.");
	}
}

int Audio::AudioCallback(
	const void *inputBuffer,
	void *outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void *userData)
{
	Audio *audio = static_cast<Audio*>(userData);
	const int16_t *in = static_cast<const int16_t*>(inputBuffer);
	int16_t *out = static_cast<int16_t*>(outputBuffer);

	memset(out, 0, sizeof(*out) * framesPerBuffer);

	int readBytes = read(
		audio->_outputPipe[0],
		out,
		sizeof(*out) * framesPerBuffer);

	if (readBytes > 0) {
		if (readBytes % 2) {
			while (read(
				audio->_outputPipe[0],
				(uint8_t*)out + readBytes,
				1) != 1) { }
		}
	}

	while (!audio->_inBuffer.IsEmpty()) {
		PlayState* ps = audio->_inBuffer.Get();

		ps->Next = audio->_activeBuffers;
		audio->_activeBuffers = ps;
	}

	PlayState **currentBuffer = &audio->_activeBuffers;

	while (*currentBuffer) {
		PlayState *buffer = *currentBuffer;

		if (buffer->Buffer->Discard) {
			buffer->Buffer->Finished = true;

			PlayState *deletedBuf = *currentBuffer;
			*currentBuffer = (*currentBuffer)->Next;
			audio->_outBuffer.Insert(deletedBuf);

			continue;
		}

		if (!buffer->Buffer->Active) {
			currentBuffer = &(*currentBuffer)->Next;
			continue;
		}

		uint64_t inIdx = buffer->Position;
		uint64_t inLen = buffer->Buffer->Data->Size();

		if (inIdx >= inLen) {
			buffer->Buffer->Finished = true;

			PlayState* deletedBuf = *currentBuffer;
			*currentBuffer = (*currentBuffer)->Next;
			audio->_outBuffer.Insert(deletedBuf);

			continue;
		}

		for (uint64_t outIdx = 0; outIdx < framesPerBuffer; ++outIdx)
		{
			out[outIdx] +=
				buffer->Buffer->Data->operator[](inIdx) *
				buffer->Buffer->Multiplier;

			++inIdx;

			if (inIdx >= inLen) {
				break;
			}
		}

		buffer->Position += framesPerBuffer;

		currentBuffer = &(*currentBuffer)->Next;
	}

	int writtenBytes = write(
		audio->_inputPipe[1],
		in,
		sizeof(*in) * framesPerBuffer);

	if (writtenBytes > 0) {
		if (writtenBytes % 2) {
			while (write(
				audio->_inputPipe[1],
				(uint8_t*)in + writtenBytes,
				1) != 1) { }
		}
	}

	return 0;
}
