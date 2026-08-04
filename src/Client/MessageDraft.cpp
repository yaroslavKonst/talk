#include "MessageDraft.hpp"

#include "../Common/Exception.hpp"

MessageDraft::MessageDraft()
{
	_currentEntry = nullptr;
	_stateStack = nullptr;
}

MessageDraft::~MessageDraft()
{
	Clear();
}

void MessageDraft::Clear()
{
	if (!_currentEntry) {
		return;
	}

	while (_currentEntry->Next) {
		DraftNode *tmp = _currentEntry->Next;
		_currentEntry->Next = _currentEntry->Next->Next;
		delete tmp->Entry;
		delete tmp;
	}

	while (_currentEntry->Previous) {
		DraftNode *tmp = _currentEntry->Previous;
		_currentEntry->Previous = _currentEntry->Previous->Previous;
		delete tmp->Entry;
		delete tmp;
	}

	delete _currentEntry->Entry;
	delete _currentEntry;
	_currentEntry = nullptr;

	while (_stateStack) {
		StateNode *tmp = _stateStack;
		_stateStack = _stateStack->Next;
		delete tmp;
	}
}

bool MessageDraft::IsEmpty()
{
	if (!_currentEntry) {
		return true;
	}

	DraftNode *node = _currentEntry;

	while (node->Previous) {
		node = node->Previous;
	}

	while (node) {
		if (node->Entry->Type == EntryType::Text) {
			DraftText *e = static_cast<DraftText*>(node->Entry);

			if (e->Editor.GetText().Length() > 0) {
				return false;
			}
		} else if (node->Entry->Type == EntryType::Attachment) {
			DraftAttachment *e =
				static_cast<DraftAttachment*>(node->Entry);

			if (e->Data.Size()) {
				return false;
			}
		}

		node = node->Next;
	}

	return true;
}

MessageDraft::DraftEntryBase *MessageDraft::GetCurrentEntry()
{
	if (!_currentEntry) {
		return nullptr;
	}

	return _currentEntry->Entry;
}

bool MessageDraft::SwitchToPreviousEntry()
{
	if (!_currentEntry) {
		return false;
	}

	if (!_currentEntry->Previous) {
		return false;
	}

	_currentEntry = _currentEntry->Previous;
	return true;
}

bool MessageDraft::SwitchToNextEntry()
{
	if (!_currentEntry) {
		return false;
	}

	if (!_currentEntry->Next) {
		return false;
	}

	_currentEntry = _currentEntry->Next;
	return true;
}

void MessageDraft::InsertNodeAfterCurrent(EntryType type)
{
	DraftNode *node = new DraftNode;

	switch (type) {
	case EntryType::Text:
		node->Entry = new DraftText;
		node->Entry->Type = EntryType::Text;
		break;
	case EntryType::Attachment:
		node->Entry = new DraftAttachment;
		node->Entry->Type = EntryType::Attachment;
		break;
	default:
		delete node;
		THROW("Unknown message draft entry type.");
	}

	if (!_currentEntry) {
		node->Next = nullptr;
		node->Previous = nullptr;
		_currentEntry = node;
		return;
	}

	node->Previous = _currentEntry;
	node->Next = _currentEntry->Next;

	if (_currentEntry->Next) {
		_currentEntry->Next->Previous = node;
	}

	_currentEntry->Next = node;

	_currentEntry = node;
}

void MessageDraft::RemoveCurrentNode()
{
	if (!_currentEntry) {
		return;
	}

	DraftNode *tmp = _currentEntry;

	if (tmp->Previous) {
		tmp->Previous->Next = tmp->Next;
	}

	if (tmp->Next) {
		tmp->Next->Previous = tmp->Previous;
	}

	if (tmp->Previous) {
		_currentEntry = tmp->Previous;
	} else if (tmp->Next) {
		_currentEntry = tmp->Next;
	} else {
		_currentEntry = nullptr;
	}

	delete tmp->Entry;
	delete tmp;
}

void MessageDraft::PushState()
{
	StateNode *node = new StateNode;
	node->Next = _stateStack;
	node->State = _currentEntry;

	_stateStack = node;
}

void MessageDraft::PopState()
{
	if (!_stateStack) {
		return;
	}

	_currentEntry = _stateStack->State;

	StateNode *tmp = _stateStack;
	_stateStack = _stateStack->Next;
	delete tmp;
}
