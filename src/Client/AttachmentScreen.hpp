#ifndef _ATTACHMENT_SCREEN_HPP
#define _ATTACHMENT_SCREEN_HPP

#include "Root.hpp"
#include "Screen.hpp"
#include "Chat.hpp"

class AttachmentScreen : public Screen
{
public:
	AttachmentScreen(Root *root, bool extract, ControlStorage *controls);

	void Redraw() override;
	Screen *ProcessEvent(int event) override;

private:
	Chat *_chat;
	bool _extract;

	String _path;
	String _status;

	bool ExtractAttachment();
	bool LoadAttachment();

	Root *_root;
	ControlStorage *_controls;
};

#endif
