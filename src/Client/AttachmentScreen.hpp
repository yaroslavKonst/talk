#ifndef _ATTACHMENT_SCREEN_HPP
#define _ATTACHMENT_SCREEN_HPP

#include "Screen.hpp"
#include "Chat.hpp"
#include "ControlStorage.hpp"

class AttachmentScreen : public Screen
{
public:
	AttachmentScreen(Chat *chat, bool extract, ControlStorage *controls);

	void Redraw() override;
	Screen *ProcessEvent(int event) override;

private:
	Chat *_chat;
	bool _extract;

	String _path;
	String _status;

	bool ExtractAttachment();
	bool LoadAttachment();

	ControlStorage *_controls;
};

#endif
