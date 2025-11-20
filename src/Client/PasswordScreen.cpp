#include "PasswordScreen.hpp"

#include <curses.h>

#include "WorkScreen.hpp"

PasswordScreen::PasswordScreen(ClientSession *session, VoiceChat *voiceChat) :
	Screen(session)
{
	_voiceChat = voiceChat;
}

void PasswordScreen::Redraw()
{
	ClearScreen();

	move(0, 0);
	addstr("Help: Exit: End | Proceed: Enter");

	if (_status.Length() > 0) {
		move(_rows / 2 + 2, _columns / 2 - _status.Length() / 2);
		addstr(_status.CStr());
	}

	move(_rows / 2, 4);
	addstr("Enter password: ");
	addstr(_password.CStr());
	refresh();
}

Screen *PasswordScreen::ProcessEvent(int event)
{
	if (event == KEY_END) {
		return nullptr;
	}

	if (event == KEY_ENTER || event == '\n') {
		if (_password.Length() == 0) {
			_status = "Password must not be empty.";
			return this;
		}

		// Transition to work screen.
		_status = "Processing...";
		Redraw();

		GenerateKeys();

		return new WorkScreen(_session, _voiceChat);
	}

	if (event == KEY_BACKSPACE || event == '\b') {
		if (_password.Length() == 0) {
			return this;
		}

		_password = _password.Substring(0, _password.Length() - 1);
		return this;
	}

	if (_password.Length() > 50) {
		_status = "Password is too long.";
		return this;
	}

	_password += event;

	return this;
}

void PasswordScreen::GenerateKeys()
{
	uint8_t salt[SALT_SIZE];
	GetSalt("talk.p.salt", salt);
	DeriveKey(_password.CStr(), salt, _session->PrivateKey);
	crypto_wipe(salt, SALT_SIZE);
	GeneratePublicKey(_session->PrivateKey, _session->PublicKey);

	GetSalt("talk.s.salt", salt);
	uint8_t seed[KEY_SIZE];
	DeriveKey(_password.CStr(), salt, seed);
	GenerateSignature(
		seed,
		_session->SignaturePrivateKey,
		_session->SignaturePublicKey);
	crypto_wipe(salt, SALT_SIZE);

	_password.Wipe();
}
