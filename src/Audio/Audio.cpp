#include "Audio.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <cstdio>

#include "../Common/Exception.hpp"

Audio::Audio()
{
	SetupPipes();
	SetupStreams();
	StartThreads();
}

Audio::~Audio()
{
	StopThreads();
	CloseStreams();
	ClosePipes();
}

void Audio::WriteRaw(CowBuffer<int16_t> sound)
{
	uint32_t bytesToWrite = sound.Size() * sizeof(int16_t);
	uint32_t writtenBytes = 0;

	do {
		int res = write(
			_writePipe[1],
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

	int res = read(_readPipe[0], result.Pointer(), bytesToRead);

	if (res <= 0) {
		THROW("Failed to read raw sound.");
	}

	if (res % 2) {
		while (read(
			_readPipe[0],
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
	return _readPipe[0];
}

void Audio::SetupStreams()
{
	pa_sample_spec sampleSpec;
	sampleSpec.format = PA_SAMPLE_S16LE;
	sampleSpec.channels = 1;
	sampleSpec.rate = SampleRate;

	pa_buffer_attr bufAttr;
	bufAttr.maxlength = -1;
	bufAttr.tlength = -1;
	bufAttr.prebuf = 1024;
	bufAttr.minreq = -1;
	bufAttr.fragsize = 2048;

	_writeConnection = pa_simple_new(
		nullptr,
		"talk",
		PA_STREAM_PLAYBACK,
		nullptr,
		"Voice output",
		&sampleSpec,
		nullptr,
		&bufAttr,
		nullptr);

	if (!_writeConnection) {
		THROW("Failed to connect to pulseaudio server.");
	}

	_readConnection = pa_simple_new(
		nullptr,
		"talk",
		PA_STREAM_RECORD,
		nullptr,
		"Voice input",
		&sampleSpec,
		nullptr,
		&bufAttr,
		nullptr);

	if (!_readConnection) {
		pa_simple_free(_writeConnection);
		THROW("Failed to connect to pulseaudio server.");
	}
}

void Audio::CloseStreams()
{
	pa_simple_free(_readConnection);

	pa_simple_drain(_writeConnection, nullptr);
	pa_simple_free(_writeConnection);
}

void Audio::SetupPipes()
{
	int res = pipe(_readPipe);

	if (res == -1) {
		THROW("Failed to create input pipe.");
	}

	res = pipe(_writePipe);

	if (res == -1) {
		THROW("Failed to create output pipe.");
	}
}

void Audio::ClosePipes()
{
	close(_readPipe[0]);
	close(_readPipe[1]);
	close(_writePipe[0]);
	close(_writePipe[1]);
}

void Audio::StartThreads()
{
	_work = true;

	int res = pthread_create(&_writeThread, nullptr, WriteLoop, this);

	if (res) {
		THROW("Failed to start thread.");
	}

	res = pthread_create(&_readThread, nullptr, ReadLoop, this);

	if (res) {
		THROW("Failed to start thread.");
	}
}

void Audio::StopThreads()
{
	_work = false;

	int16_t s = 0;
	write(_writePipe[1], &s, sizeof(s));

	pthread_join(_readThread, nullptr);
	pthread_join(_writeThread, nullptr);
}

void *Audio::WriteLoop(void *arg)
{
	Audio *audio = static_cast<Audio*>(arg);
	int bufferSize = 512;

	int16_t *buffer = new int16_t[bufferSize];

	while (audio->_work) {
		int res = read(
			audio->_writePipe[0],
			buffer,
			bufferSize * sizeof(int16_t));

		if (res > 0) {
			if (res % 2) {
				while (read(
					audio->_writePipe[0],
					(char*)buffer + res,
					1) != 1) { }
				++res;
			}
		}

		pa_simple_write(audio->_writeConnection, buffer, res, nullptr);
	}

	delete[] buffer;
	return nullptr;
}

void *Audio::ReadLoop(void *arg)
{
	Audio *audio = static_cast<Audio*>(arg);
	int bufferSize = 512;

	int16_t *buffer = new int16_t[bufferSize];

	while (audio->_work) {
		pa_simple_read(
			audio->_readConnection,
			buffer,
			bufferSize * sizeof(int16_t),
			nullptr);

		int writtenBytes = 0;

		do {
			int res = write(
				audio->_readPipe[1],
				(char*)buffer + writtenBytes,
				bufferSize * sizeof(int16_t) - writtenBytes);

			if (res > 0) {
				writtenBytes += res;
			} else {
				break;
			}
		} while (writtenBytes < bufferSize * (int)sizeof(int16_t));
	}

	delete[] buffer;
	return nullptr;
}
