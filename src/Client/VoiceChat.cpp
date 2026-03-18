#include "VoiceChat.hpp"

#include <cstdlib>
#include <cstring>
#include <curses.h>

#include "../Common/Hex.hpp"
#include "TextColor.hpp"

// Filters.
// LPF.
struct LPFData
{
	int32_t Size;
	int16_t *Buffer;

	int Position;
	int bf;
	int bfSize;

	LPFData(int size)
	{
		Size = size;
		Buffer = new int16_t[Size];
		Position = 0;
		bf = 0;
		bfSize = 0;
	}

	~LPFData()
	{
		delete Buffer;
	}
};

static void LowPassFilter(
	int16_t *buffer,
	int size,
	LPFData *data)
{
	for (int i = 0; i < size; i++) {
		if (data->bfSize == data->Size) {
			data->bf -= data->Buffer[data->Position];
			data->bfSize -= 1;
		}

		data->Buffer[data->Position] = buffer[i];
		data->bf += buffer[i];
		data->bfSize += 1;

		data->Position += 1;

		if (data->Position >= data->Size) {
			data->Position = 0;
		}

		buffer[i] = data->bf / data->bfSize;
	}
}

// Voice chat.
VoiceChat::VoiceChat(Root *root)
{
	_root = root;

	_state = VoiceStateOff;
	_silence = true;
	_mute = false;

	_volume = 100;
	_applyFilter = true;
	_silenceLevel = 3;
	_silenceSlope = 0;

	_configFile = nullptr;
}

VoiceChat::~VoiceChat()
{
	Stop();
}

void VoiceChat::SetConfigFile(IniFile *configFile)
{
	_configFile = configFile;
	LoadConfigFile();
}

void VoiceChat::ProcessInput()
{
	CowBuffer<int16_t> audioData = _audio.ReadRaw();

	if (_state != VoiceStateActive) {
		return;
	}

	if (_mute) {
		_silenceSlope = 0;
		return;
	}

	int16_t absMax = 0;

	for (uint32_t i = 0; i < audioData.Size(); i++) {
		int16_t value = abs(audioData[i]);

		if (value > absMax) {
			absMax = value;
		}
	}

	int silenceLevel = _silenceLevel * (65535 / 2 - 1) / 100;

	if (absMax < silenceLevel) {
		if (!_silence) {
			_silence = true;
			_root->Ui->Redraw();
		}

		--_silenceSlope;
	} else {
		if (_silence) {
			_silence = false;
			_root->Ui->Redraw();
		}

		_silenceSlope += 50;
	}


	if (_silenceSlope > 100) {
		_silenceSlope = 100;
	} else if (_silenceSlope < 0) {
		_silenceSlope = 0;
	}

	if (!_silenceSlope) {
		return;
	}

	for (unsigned int i = 0; i < audioData.Size(); i++) {
		audioData[i] = audioData[i] * _silenceSlope / 100;
	}

	_root->Network->SendVoiceFrame(EncryptSoundFrame(audioData));
}

bool VoiceChat::Active()
{
	return _state != VoiceStateOff;
}

void VoiceChat::Prepare(
	String name,
	const uint8_t *peerKey,
	int64_t timestamp,
	bool invert,
	const uint8_t *privateKey,
	const uint8_t *publicKey)
{
	_peerName = name;

	_state = VoiceStateInit;

	if (!invert) {
		GenerateSessionKeys(
			privateKey,
			publicKey,
			peerKey,
			timestamp,
			_outES.Key,
			_inES.Key,
			invert);
	} else {
		GenerateSessionKeys(
			privateKey,
			publicKey,
			peerKey,
			timestamp,
			_inES.Key,
			_outES.Key,
			invert);
	}

	InitNonce(_outES.Nonce);
	memset(_inES.Nonce, 0, NONCE_SIZE);
}

void VoiceChat::Ask()
{
	_state = VoiceStateAsk;
}

void VoiceChat::Wait()
{
	_state = VoiceStateWait;
}

void VoiceChat::Start()
{
	_state = VoiceStateActive;
}

void VoiceChat::Stop()
{
	_state = VoiceStateOff;
	_mute = false;
	crypto_wipe(_outES.Key, KEY_SIZE);
	crypto_wipe(_inES.Key, KEY_SIZE);
}

bool VoiceChat::ReceiveVoiceFrame(CowBuffer<uint8_t> frame)
{
	if (_state != VoiceStateActive) {
		return true;
	}

	CowBuffer<int16_t> audioData = DecryptSoundFrame(frame);

	if (audioData.Size() == 0) {
		Stop();
		_root->Network->EndVoice();
		return false;
	}

	if (_applyFilter) {
		static LPFData data(5);

		LowPassFilter(
			audioData.Pointer(),
			audioData.Size(),
			&data);
	}

	if (_volume != 100) {
		for (unsigned int i = 0; i < audioData.Size(); i++) {
			audioData[i] = audioData[i] * _volume / 100;
		}
	}

	_audio.WriteRaw(audioData);
	return true;
}

int VoiceChat::GetSoundReadFileDescriptor()
{
	return _audio.GetSoundReadFileDescriptor();
}

String VoiceChat::GetPeerName()
{
	return _peerName;
}

bool VoiceChat::IsMuted()
{
	return _mute;
}

void VoiceChat::ToggleMute()
{
	_mute = !_mute;
}

int VoiceChat::GetVolume()
{
	return _volume;
}

void VoiceChat::IncreaseVolume()
{
	++_volume;

	if (_volume > 200) {
		_volume = 200;
	}
}

void VoiceChat::DecreaseVolume()
{
	--_volume;

	if (_volume <= 0) {
		_volume = 1;
	}
}

int VoiceChat::GetSilenceLevel()
{
	return _silenceLevel;
}

void VoiceChat::IncreaseSilenceLevel()
{
	++_silenceLevel;

	if (_silenceLevel > 100) {
		_silenceLevel = 100;
	}
}

void VoiceChat::DecreaseSilenceLevel()
{
	--_silenceLevel;

	if (_silenceLevel <= 0) {
		_silenceLevel = 1;
	}
}

bool VoiceChat::GetFilterEnabled()
{
	return _applyFilter;
}

void VoiceChat::EnableFilter()
{
	_applyFilter = true;
}

void VoiceChat::DisableFilter()
{
	_applyFilter = false;
}

void VoiceChat::LoadConfigFile()
{
	String volumeStr = _configFile->Get("voice", "Volume");
	String applyFilterStr = _configFile->Get("voice", "ApplyFilter");
	String silenceLevelStr = _configFile->Get("voice", "SilenceLevel");

	if (volumeStr.Length()) {
		int volume = atoi(volumeStr.CStr());

		if (volume > 0) {
			_volume = volume;

			if (_volume > 200) {
				_volume = 200;
			}
		}

	}

	if (applyFilterStr.Length()) {
		if (applyFilterStr == "Yes") {
			_applyFilter = true;
		} else if (applyFilterStr == "No") {
			_applyFilter = false;
		}
	}

	if (silenceLevelStr.Length()) {
		int silenceLevel = atoi(silenceLevelStr.CStr());

		if (silenceLevel > 0) {
			_silenceLevel = silenceLevel;

			if (_silenceLevel > 100) {
				_silenceLevel = 100;
			}
		}
	}
}

void VoiceChat::UpdateConfigFile()
{
	_configFile->Set("voice", "Volume", ToString(_volume));
	_configFile->Set("voice", "ApplyFilter", _applyFilter ? "Yes" : "No");
	_configFile->Set("voice", "SilenceLevel", ToString(_silenceLevel));
	_configFile->Write();
}

CowBuffer<uint8_t> VoiceChat::EncryptSoundFrame(CowBuffer<int16_t> frame)
{
	CowBuffer<uint8_t> input(frame.Size() * sizeof(int16_t));
	memcpy(input.Pointer(), frame.Pointer(), input.Size());

	return Encrypt(input, _outES);
}

CowBuffer<int16_t> VoiceChat::DecryptSoundFrame(CowBuffer<uint8_t> frame)
{
	CowBuffer<uint8_t> decryptedData = Decrypt(frame, _inES);

	if (!decryptedData.Size()) {
		return CowBuffer<int16_t>();
	}

	CowBuffer<int16_t> result(decryptedData.Size() / sizeof(int16_t));

	memcpy(result.Pointer(), decryptedData.Pointer(), decryptedData.Size());
	return result;
}
