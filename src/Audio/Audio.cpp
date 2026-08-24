#include "Audio.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <cstdio>

#include "../Common/Exception.hpp"
#include "../Common/StreamReader.hpp"
#include "../Common/StreamWriter.hpp"

AudioRecorder::AudioRecorder()
{
	SetupPipe();

	try {
		SetupStream();
	} catch(...) {
		CloseMainPipeEnd();
		CloseThreadPipeEnd();
		throw;
	}

	try {
		StartThread();
	} catch (...) {
		CloseThreadPipeEnd();
		CloseMainPipeEnd();
		CloseStream();
		throw;
	}
}

AudioRecorder::~AudioRecorder()
{
	CloseMainPipeEnd();
	JoinThread();
	CloseThreadPipeEnd();
	CloseStream();
}

CowBuffer<int16_t> AudioRecorder::ReadRaw()
{
	StreamReader reader(_readPipe[0], SampleCount * sizeof(int16_t));

	while (!reader.ReadingEnd()) {
		bool success = reader.Read();

		if (!success) {
			THROW("Failed to read raw sound.");
		}
	}

	const CowBuffer<uint8_t> bytes = reader.GetBuffer();
	CowBuffer<int16_t> sound(SampleCount);
	memcpy(sound.Pointer(), bytes.Pointer(), bytes.Size());

	return sound;
}

int AudioRecorder::GetSoundReadFileDescriptor()
{
	return _readPipe[0];
}

void AudioRecorder::SetupStream()
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
		THROW("Failed to connect to pulseaudio server.");
	}
}

void AudioRecorder::CloseStream()
{
	pa_simple_free(_readConnection);
}

void AudioRecorder::SetupPipe()
{
	int res = pipe(_readPipe);

	if (res == -1) {
		THROW("Failed to create input pipe.");
	}
}

void AudioRecorder::CloseMainPipeEnd()
{
	close(_readPipe[0]);
}

void AudioRecorder::CloseThreadPipeEnd()
{
	close(_readPipe[1]);
}

void AudioRecorder::StartThread()
{
	int res = pthread_create(&_readThread, nullptr, ReadLoop, this);

	if (res) {
		THROW("Failed to start thread.");
	}
}

void AudioRecorder::JoinThread()
{
	pthread_join(_readThread, nullptr);
}

void *AudioRecorder::ReadLoop(void *arg)
{
	AudioRecorder *audio = static_cast<AudioRecorder*>(arg);

	CowBuffer<uint8_t> buffer(SampleCount * sizeof(int16_t));
	bool work = true;

	while (work) {
		int paReadStat = pa_simple_read(
			audio->_readConnection,
			buffer.Pointer(),
			buffer.Size(),
			nullptr);

		if (paReadStat) {
			break;
		}

		StreamWriter writer(audio->_readPipe[1], buffer);

		while (!writer.WritingEnd()) {
			bool success = writer.Write();

			if (!success) {
				work = false;
				break;
			}
		}
	}

	return nullptr;
}

AudioPlayback::AudioPlayback()
{
	SetupPipe();

	try {
		SetupStream();
	} catch(...) {
		CloseMainPipeEnd();
		CloseThreadPipeEnd();
		throw;
	}

	try {
		StartThread();
	} catch (...) {
		CloseThreadPipeEnd();
		CloseMainPipeEnd();
		CloseStream();
		throw;
	}
}

AudioPlayback::~AudioPlayback()
{
	CloseMainPipeEnd();
	JoinThread();
	CloseThreadPipeEnd();
	CloseStream();
}

void AudioPlayback::WriteRaw(const CowBuffer<int16_t> sound)
{
	CowBuffer<uint8_t> bytes(sound.Size() * sizeof(int16_t));
	memcpy(bytes.Pointer(), sound.Pointer(), bytes.Size());

	StreamWriter writer(_writePipe[1], bytes);

	while (!writer.WritingEnd()) {
		bool success = writer.Write();

		if (!success) {
			THROW("Failed to write raw sound.");
		}
	}
}

void AudioPlayback::SetupStream()
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
}

void AudioPlayback::CloseStream()
{
	pa_simple_drain(_writeConnection, nullptr);
	pa_simple_free(_writeConnection);
}

void AudioPlayback::SetupPipe()
{
	int res = pipe(_writePipe);

	if (res == -1) {
		THROW("Failed to create output pipe.");
	}
}

void AudioPlayback::CloseMainPipeEnd()
{
	close(_writePipe[1]);
}

void AudioPlayback::CloseThreadPipeEnd()
{
	close(_writePipe[0]);
}

void AudioPlayback::StartThread()
{
	int res = pthread_create(&_writeThread, nullptr, WriteLoop, this);

	if (res) {
		THROW("Failed to start thread.");
	}
}

void AudioPlayback::JoinThread()
{
	pthread_join(_writeThread, nullptr);
}

void *AudioPlayback::WriteLoop(void *arg)
{
	AudioPlayback *audio = static_cast<AudioPlayback*>(arg);

	bool work = true;

	while (work) {
		StreamReader reader(
			audio->_writePipe[0],
			SampleCount * sizeof(int16_t));

		while (!reader.ReadingEnd()) {
			bool success = reader.Read();

			if (!success) {
				work = false;
				break;
			}
		}

		if (!work) {
			break;
		}

		const CowBuffer<uint8_t> buffer = reader.GetBuffer();

		pa_simple_write(
			audio->_writeConnection,
			buffer.Pointer(),
			buffer.Size(),
			nullptr);
	}

	return nullptr;
}
