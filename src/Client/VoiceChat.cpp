#include "VoiceChat.hpp"

#include <cstdlib>
#include <curses.h>

#include "../Common/Hex.hpp"
#include "TextColor.hpp"

// Filters
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

VoiceChat::VoiceChat()
{
	_state = VoiceStateOff;
	_silence = true;
	_mute = false;

	_volume = 100;
	_applyFilter = true;
	_silenceLevel = 3;

	_configFile = nullptr;
	_settingsMode = false;
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

void VoiceChat::StartSettings()
{
	_settingsMode = true;
}

void VoiceChat::ProcessInput()
{
	CowBuffer<int16_t> audioData = _audio.ReadRaw();

	if (_state != VoiceStateActive) {
		return;
	}

	if (!_voiceProcessor) {
		return;
	}

	if (_mute) {
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
			_voiceProcessor->VoiceRedrawRequested();
		}

		return;
	}

	if (_silence) {
		_silence = false;
		_voiceProcessor->VoiceRedrawRequested();
	}

	_voiceProcessor->SendVoiceFrame(EncryptSoundFrame(audioData));
}

void VoiceChat::Redraw(int rows, int columns)
{
	int tmpY;
	int tmpX;
	getyx(stdscr, tmpY, tmpX);
	RedrawState(rows, columns);
	move(tmpY, tmpX);

	if (_settingsMode) {
		RedrawSettings(rows, columns);
	}

	if (_state != VoiceStateAsk) {
		return;
	}

	String prompt = "Incomng call.";
	String choice = "Press 'y' to answer, 'n' to decline call.";

	int messageSize = choice.Length();

	if (_peerName.Length() > messageSize) {
		messageSize = _peerName.Length();
	}

	int frameSize = messageSize + 2;

	if (frameSize < 30) {
		frameSize = 30;
	}

	int baseY = rows / 2 - 4;
	int limitY = rows / 2 + 4;

	int baseX = columns / 2 - frameSize / 2 - 2;
	int limitX = columns / 2 + frameSize / 2 + 3;

	// Clear.
	for (int r = baseY; r < limitY; r++) {
		for (int c = baseX; c < limitX; c++) {
			move(r, c);
			addch(' ');
		}
	}

	// Frame.
	attrset(COLOR_PAIR(YELLOW_TEXT));

	for (int r = baseY + 2; r < limitY - 2; r++) {
		move(r, baseX + 1);
		addch(ACS_VLINE);

		move(r, limitX - 2);
		addch(ACS_VLINE);
	}

	for (int c = baseX + 2; c < limitX - 2; c++) {
		move(baseY + 1, c);
		addch(ACS_HLINE);

		move(limitY - 2, c);
		addch(ACS_HLINE);
	}

	move(baseY + 1, baseX + 1);
	addch(ACS_ULCORNER);
	move(baseY + 1, limitX - 2);
	addch(ACS_URCORNER);
	move(limitY - 2, baseX + 1);
	addch(ACS_LLCORNER);
	move(limitY - 2, limitX - 2);
	addch(ACS_LRCORNER);

	move(baseY + 1, baseX + 2);
	addstr("Incoming voice call");

	attrset(COLOR_PAIR(DEFAULT_TEXT));

	// Message.
	move(baseY + 2, columns / 2 - prompt.Length() / 2);
	addstr(prompt.CStr());

	move(baseY + 3, columns / 2 - _peerName.Length() / 2);
	addstr(_peerName.CStr());

	move(baseY + 5, columns / 2 - choice.Length() / 2);
	addstr(choice.CStr());
}

bool VoiceChat::ProcessEvent(int event)
{
	bool settingsMode = _settingsMode;

	if (_settingsMode) {
		ProcessSettings(event);
	}

	if (_state == VoiceStateOff) {
		return settingsMode;
	}

	if (_state == VoiceStateAsk) {
		if (event == 'y') {
			_state = VoiceStateActive;
			_voiceProcessor->AnswerVoiceRequest(true);
		} else if (event == 'n') {
			_state = VoiceStateOff;
			_voiceProcessor->AnswerVoiceRequest(false);
		}

		return true;
	}

	if (event == 'v' - 'a' + 1) {
		Stop();
		_voiceProcessor->EndVoice();
		return true;
	}

	if (event == 'b' - 'a' + 1) {
		if (_state == VoiceStateActive) {
			_mute = !_mute;
			return true;
		}
	}

	return settingsMode;
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
		_voiceProcessor->EndVoice();
		return false;
	}

	if (_applyFilter) {
		static LPFData *data = nullptr;

		if (!data) {
			data = new LPFData(5);
		}

		LowPassFilter(
			audioData.Pointer(),
			audioData.Size(),
			data);
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

void VoiceChat::RedrawState(int rows, int columns)
{
	move(rows - 1, 0);
	addstr("Voice status: ");

	switch (_state) {
	case VoiceStateOff:
		addstr("not connected");
		break;
	case VoiceStateInit:
		attrset(COLOR_PAIR(YELLOW_TEXT));
		addstr("initializing connection");
		break;
	case VoiceStateAsk:
		attrset(COLOR_PAIR(YELLOW_TEXT));
		addstr("please respond");
		break;
	case VoiceStateWait:
		attrset(COLOR_PAIR(YELLOW_TEXT));
		addstr("waiting for answer");
		break;
	case VoiceStateActive:
		attrset(COLOR_PAIR(GREEN_TEXT));
		addstr("active");

		if (_mute) {
			attrset(COLOR_PAIR(RED_TEXT));
			addstr(" (mute)");
			attrset(COLOR_PAIR(GREEN_TEXT));
		} else if (_silence) {
			addstr(" (silence)");
		}
		break;
	}

	int y;
	int x;

	getyx(stdscr, y, x);

	String name = _peerName;

	if (name.Length() > columns - x - 5) {
		name = name.Substring(0, columns - x - 5) + "...";
	}

	if (_state != VoiceStateOff) {
		addstr(" (");
		addstr(name.CStr());
		addch(')');
	}

	attrset(COLOR_PAIR(DEFAULT_TEXT));
	addch('.');
}

void VoiceChat::RedrawSettings(int rows, int columns)
{
	for (int r = 0; r < rows - 1; r++) {
		for (int c = 0; c < columns; c++) {
			move(r, c);
			addch(' ');
		}
	}

	move(0, 0);
	addstr("Exit: End");

	move(rows / 2 - 4, columns / 4 - 3);
	addstr("Volume");
	move(rows / 2 - 2, columns / 4 - 1);
	addstr("'q'");
	move(rows / 2, columns / 4 - 1);
	addstr(ToString(_volume).CStr());
	move(rows / 2 + 2, columns / 4 - 1);
	addstr("'a'");

	move(rows / 2 - 4, columns / 2 - 6);
	addstr("Silence level");
	move(rows / 2 - 2, columns / 2 - 1);
	addstr("'w'");
	move(rows / 2, columns / 2 - 1);
	addstr(ToString(_silenceLevel).CStr());
	move(rows / 2 + 2, columns / 2 - 1);
	addstr("'s'");

	move(rows / 2 - 4, columns * 3 / 4 - 3);
	addstr("Filter");
	move(rows / 2 - 2, columns * 3 / 4 - 1);
	addstr("'e'");
	move(rows / 2, columns * 3 / 4 - 1);
	addstr(_applyFilter ? "Yes" : "No");
	move(rows / 2 + 2, columns * 3 / 4 - 1);
	addstr("'d'");

	move(1, 0);
}

void VoiceChat::ProcessSettings(int event)
{
	switch (event) {
	case KEY_END:
		_settingsMode = false;
		UpdateConfigFile();
		break;
	case 'q':
		++_volume;

		if (_volume > 200) {
			_volume = 200;
		}

		break;
	case 'a':
		--_volume;

		if (_volume <= 0) {
			_volume = 1;
		}

		break;
	case 'w':
		++_silenceLevel;

		if (_silenceLevel > 100) {
			_silenceLevel = 100;
		}

		break;
	case 's':
		--_silenceLevel;

		if (_silenceLevel <= 0) {
			_silenceLevel = 1;
		}

		break;
	case 'e':
		_applyFilter = true;
		break;
	case 'd':
		_applyFilter = false;
		break;
	}
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
