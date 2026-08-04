#ifndef _MESSAGE_DRAFT_HPP
#define _MESSAGE_DRAFT_HPP

#include "TextEditor.hpp"
#include "../Common/CowBuffer.hpp"

class MessageDraft
{
public:
	MessageDraft();
	~MessageDraft();

	enum class EntryType
	{
		Text,
		Attachment
	};

	struct DraftEntryBase
	{
	public:
		EntryType Type;

		virtual ~DraftEntryBase()
		{ }
	};

	struct DraftText : public DraftEntryBase
	{
		TextEditor Editor;
	};

	struct DraftAttachment : public DraftEntryBase
	{
		String Name;
		CowBuffer<uint8_t> Data;
	};

	MessageDraft(const MessageDraft &draft) = delete;
	MessageDraft &operator=(const MessageDraft &draft) = delete;

	void Clear();
	bool IsEmpty();

	DraftEntryBase *GetCurrentEntry();
	bool SwitchToPreviousEntry();
	bool SwitchToNextEntry();

	void InsertNodeAfterCurrent(EntryType type);
	void RemoveCurrentNode();

	void PushState();
	void PopState();

private:
	struct DraftNode
	{
		DraftNode *Next;
		DraftNode *Previous;
		DraftEntryBase *Entry;
	};

	DraftNode *_currentEntry;

	struct StateNode
	{
		StateNode *Next;
		DraftNode *State;
	};

	StateNode *_stateStack;
};

#endif
