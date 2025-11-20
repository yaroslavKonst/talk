#ifndef _AUDIO_HPP
#define _AUDIO_HPP

#include <portaudio.h>

#include "../Common/RingBuffer.hpp"
#include "../Common/CowBuffer.hpp"

class Audio
{
public:
	static const int SampleRate = 44100;
	static const int SampleCount = 512;

	struct Sound
	{
		CowBuffer<int16_t> *Data;

		float Multiplier;
		volatile bool Finished;
		volatile bool Active;
		volatile bool Discard;

		Sound(CowBuffer<int16_t> *data)
		{
			Data = data;

			Multiplier = 1.0f;
			Finished = false;
			Active = true;
			Discard = false;
		}
	};

	Audio();
	~Audio();

	void Submit(Sound *buffer);

	void WriteRaw(CowBuffer<int16_t> sound);
	CowBuffer<int16_t> ReadRaw();
	int GetSoundReadFileDescriptor();

private:
	struct PlayState
	{
		Sound *Buffer;
		uint64_t Position;

		PlayState* Next;

		PlayState(Sound *buffer)
		{
			Buffer = buffer;
			Position = 0;
			Next = nullptr;
		}
	};

	PaStream *_stream;
	void SetupStream();
	void CloseStream();

	PlayState *_activeBuffers;

	RingBuffer<PlayState*> _inBuffer;
	RingBuffer<PlayState*> _outBuffer;

	int _inputPipe[2];
	int _outputPipe[2];
	void SetupPipes();
	void ClosePipes();

	void MakeNonblocking(int fd);

	static int AudioCallback(
		const void *inputBuffer,
		void *outputBuffer,
		unsigned long framesPerBuffer,
		const PaStreamCallbackTimeInfo *timeInfo,
		PaStreamCallbackFlags statusFlags,
		void *userData);
};

#endif
