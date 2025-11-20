#include "VoiceChat.hpp"

#include <cstdlib>
#include <curses.h>

#include "../Common/Hex.hpp"
#include "TextColor.hpp"

VoiceChat::VoiceChat()
{
	_state = VoiceStateOff;
	_silence = true;
	_mute = false;
}

VoiceChat::~VoiceChat()
{
	Stop();
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

	if (absMax < 1000) {
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
	if (_state == VoiceStateOff) {
		return false;
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

	return false;
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
