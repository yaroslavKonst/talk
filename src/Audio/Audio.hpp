#ifndef _AUDIO_HPP
#define _AUDIO_HPP

#include <pulse/simple.h>
#include <pthread.h>

#include "../Common/CowBuffer.hpp"

class Audio
{
public:
	static const int SampleRate = 44100;
	static const int SampleCount = 512;

	Audio();
	~Audio();

	void WriteRaw(CowBuffer<int16_t> sound);
	CowBuffer<int16_t> ReadRaw();
	int GetSoundReadFileDescriptor();

private:
	pa_simple *_writeConnection;;
	pa_simple *_readConnection;;
	void SetupStreams();
	void CloseStreams();

	int _readPipe[2];
	int _writePipe[2];
	void SetupPipes();
	void ClosePipes();

	pthread_t _readThread;
	pthread_t _writeThread;
	volatile bool _work;
	void StartThreads();
	void StopThreads();

	static void *WriteLoop(void *arg);
	static void *ReadLoop(void *arg);
};

#endif
