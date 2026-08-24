#ifndef _AUDIO_HPP
#define _AUDIO_HPP

#include <pulse/simple.h>
#include <pthread.h>

#include "../Common/CowBuffer.hpp"

class AudioRecorder
{
public:
	static const int SampleRate = 44100;
	static const int SampleCount = 512;

	AudioRecorder();
	~AudioRecorder();

	CowBuffer<int16_t> ReadRaw();
	int GetSoundReadFileDescriptor();

private:
	pa_simple *_readConnection;
	void SetupStream();
	void CloseStream();

	int _readPipe[2];
	void SetupPipe();
	void CloseMainPipeEnd();
	void CloseThreadPipeEnd();

	pthread_t _readThread;
	void StartThread();
	void JoinThread();

	static void *ReadLoop(void *arg);
};

class AudioPlayback
{
public:
	static const int SampleRate = 44100;
	static const int SampleCount = 512;

	AudioPlayback();
	~AudioPlayback();

	void WriteRaw(const CowBuffer<int16_t> sound);

private:
	pa_simple *_writeConnection;
	void SetupStream();
	void CloseStream();

	int _writePipe[2];
	void SetupPipe();
	void CloseMainPipeEnd();
	void CloseThreadPipeEnd();

	pthread_t _writeThread;
	void StartThread();
	void JoinThread();

	static void *WriteLoop(void *arg);
};

#endif
