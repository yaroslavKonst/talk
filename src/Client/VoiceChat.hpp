#ifndef _VOICE_CHAT_HPP
#define _VOICE_CHAT_HPP

#include "../Audio/Audio.hpp"
#include "../Crypto/Crypto.hpp"

class VoiceProcessor
{
public:
	virtual ~VoiceProcessor()
	{ }

	virtual void SendVoiceFrame(CowBuffer<uint8_t> frame) = 0;
	virtual void AnswerVoiceRequest(bool accept) = 0;
	virtual void EndVoice() = 0;
	virtual void VoiceRedrawRequested() = 0;
};

class VoiceChat
{
public:
	VoiceChat();
	~VoiceChat();

	void RegisterProcessor(VoiceProcessor *processor)
	{
		_voiceProcessor = processor;
	}

	// Process microphone input.
	void ProcessInput();

	// Process GUI.
	void Redraw(int rows, int columns);
	bool ProcessEvent(int event);

	// Control.
	bool Active();
	void Prepare(
		String name,
		const uint8_t *peerKey,
		int64_t timestamp,
		bool invert,
		const uint8_t *privateKey,
		const uint8_t *publicKey);
	void Ask();
	void Wait();
	void Start();
	void Stop();

	bool ReceiveVoiceFrame(CowBuffer<uint8_t> frame);

	int GetSoundReadFileDescriptor();

private:
	void RedrawState(int rows, int columns);

	enum VoiceChatState
	{
		VoiceStateOff = 0,
		VoiceStateInit = 1,
		VoiceStateAsk = 2,
		VoiceStateWait = 3,
		VoiceStateActive = 4
	};

	VoiceChatState _state;

	Audio _audio;

	VoiceProcessor *_voiceProcessor;

	String _peerName;

	EncryptedStream _outES;
	EncryptedStream _inES;

	CowBuffer<uint8_t> EncryptSoundFrame(CowBuffer<int16_t> frame);
	CowBuffer<int16_t> DecryptSoundFrame(CowBuffer<uint8_t> frame);

	bool _silence;
};

#endif
