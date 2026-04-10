#ifndef _VOICE_CHAT_HPP
#define _VOICE_CHAT_HPP

#include "Root.hpp"
#include "../Audio/Audio.hpp"
#include "../Crypto/Crypto.hpp"
#include "../Common/IniFile.hpp"

class VoiceChat : public VoiceEventProcessor
{
public:
	VoiceChat(Root *root);
	~VoiceChat();

	void SetConfigFile(IniFile *configFile);

	// Process microphone input.
	void ProcessInput();

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

	String GetPeerName() override;
	bool IsMuted() override;
	bool IsSilent() override;
	VoiceState GetState() override;

	void ToggleMute() override;

	int GetVolume();
	void IncreaseVolume();
	void DecreaseVolume();

	int GetSilenceLevel();
	void IncreaseSilenceLevel();
	void DecreaseSilenceLevel();

	bool GetFilterEnabled();
	void EnableFilter();
	void DisableFilter();

private:
	Root *_root;

	IniFile *_configFile;
	void LoadConfigFile();
	void UpdateConfigFile();

	VoiceState _state;

	Audio _audio;

	String _peerName;

	EncryptedStream _outES;
	EncryptedStream _inES;

	CowBuffer<uint8_t> EncryptSoundFrame(CowBuffer<int16_t> frame);
	CowBuffer<int16_t> DecryptSoundFrame(CowBuffer<uint8_t> frame);

	bool _silence;
	int _silenceSlope;
	bool _mute;

	int _volume;
	bool _applyFilter;
	int _silenceLevel;
};

#endif
