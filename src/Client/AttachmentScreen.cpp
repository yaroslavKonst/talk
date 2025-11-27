#include "AttachmentScreen.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <curses.h>

AttachmentScreen::AttachmentScreen(Chat *chat, bool extract) : Screen(nullptr)
{
	_chat = chat;
	_extract = extract;
}

void AttachmentScreen::Redraw()
{
	ClearScreen();

	move(0, 0);
	addstr("Exit: End | Proceed: Enter");

	if (!_extract) {
		addstr(" | Remove attachment: Delete");
	}

	if (_status.Length()) {
		move(_rows / 2 + 2, _columns / 2 - _status.Length() / 2);
		addstr(_status.CStr());
	}

	move(_rows / 2, 2);
	if (_extract) {
		addstr("Extraction path: ");
	} else {
		addstr("Loading path: ");
	}

	addstr(_path.CStr());

	refresh();
}

Screen *AttachmentScreen::ProcessEvent(int event)
{
	if (event == KEY_END) {
		return nullptr;
	}

	if (event == KEY_ENTER || event == '\n') {
		// Proceed.
		_status = "Processing...";
		Redraw();

		bool success;

		if (_extract) {
			success = ExtractAttachment();
		} else {
			success = LoadAttachment();
		}

		if (success) {
			return nullptr;
		}

		return this;
	}

	if (event == KEY_DC) {
		if (!_extract) {
			_chat->ClearAttachment();
			return nullptr;
		}

		return this;
	}

	if (event == KEY_BACKSPACE || event == '\b') {
		if (!_path.Length()) {
			return this;
		}

		if (_path.Length() > 1) {
			_path = _path.Substring(0, _path.Length() - 1);
		} else {
			_path.Clear();
		}

		return this;
	}

	if (event < ' ' || event > '~') {
		_status = "Illegal character.";
		return this;
	}

	_path += event;
	return this;
}

bool AttachmentScreen::ExtractAttachment()
{
	if (!_chat->HasAttachment()) {
		_status = "Selected message does not have an attachment.";
		return false;
	}

	const CowBuffer<uint8_t> attachment = _chat->ExtractAttachment();
	int fd;

	for (;;) {
		fd = open(
			_path.CStr(),
			O_WRONLY | O_CREAT | O_TRUNC,
			0600);

		if (fd == -1) {
			if (errno == EINTR) {
				continue;
			} else if (errno == EACCES) {
				_status = "Access denied.";
				return false;
			} else if (errno == EDQUOT) {
				_status = "Disk space quota is exhausted.";
				return false;
			} else if (errno == EINVAL) {
				_status = "Invalid file name.";
				return false;
			} else if (errno == EISDIR) {
				_status = "Existing directory is specified.";
				return false;
			} else if (errno == ENOSPC) {
				_status = "No free space.";
				return false;
			} else {
				_status = "Error";
				return false;
			}
		} else {
			break;
		}
	}

	uint64_t writtenBytes = 0;

	while (writtenBytes < attachment.Size()) {
		int64_t wb = write(
			fd,
			attachment.Pointer(writtenBytes),
			attachment.Size() - writtenBytes);

		if (wb > 0) {
			writtenBytes += wb;
		} else if (wb == -1) {
			if (errno == EINTR) {
				continue;
			} else if (errno == EDQUOT) {
				close(fd);
				_status = "Disk space quota is exhausted.";
				return false;
			} else if (errno == EIO) {
				close(fd);
				_status = "Low-level I/O fault.";
				return false;
			} else if (errno == ENOSPC) {
				close(fd);
				_status = "No free space left on device.";
				return false;
			}

			close(fd);
			_status = "Error.";
			return false;
		}
	}

	close(fd);
	return true;
}

bool AttachmentScreen::LoadAttachment()
{
	int fd;

	for (;;) {
		fd = open(_path.CStr(), O_RDONLY);

		if (fd == -1) {
			if (errno == EINTR) {
				continue;
			} else if (errno == EACCES) {
				_status = "Access denied.";
				return false;
			} else if (errno == EDQUOT) {
				_status = "Disk space quota is exhausted.";
				return false;
			} else if (errno == EINVAL) {
				_status = "Invalid file name.";
				return false;
			} else if (errno == EISDIR) {
				_status = "Existing directory is specified.";
				return false;
			} else if (errno == ENOSPC) {
				_status = "No free space.";
				return false;
			} else if (errno == ENOENT) {
				_status = "File does not exist.";
				return false;
			} else {
				_status = "Error";
				return false;
			}
		} else {
			break;
		}
	}

	int64_t fileSize = lseek(fd, 0, SEEK_END);

	if (fileSize == -1) {
		_status = "Failed to get file size.";
		close(fd);
		return false;
	}

	if (fileSize > 1024 * 1024 * 896) {
		_status = "File is too big.";
		close(fd);
		return false;
	}

	uint64_t readBytes = 0;
	CowBuffer<uint8_t> attachment(fileSize);

	int status = lseek(fd, 0, SEEK_SET);

	if (status == -1) {
		_status = "Failed to seek to file start.";
		close(fd);
		return false;
	}

	while (readBytes < attachment.Size()) {
		int64_t rb = read(
			fd,
			attachment.Pointer(readBytes),
			attachment.Size() - readBytes);

		if (rb > 0) {
			readBytes += rb;
		} else if (rb == -1) {
			if (errno == EINTR) {
				continue;
			} else if (errno == EIO) {
				close(fd);
				_status = "Low-level I/O fault.";
				return false;
			}

			close(fd);
			_status = "Error.";
			return false;
		} else {
			_status = "File ended.";
			close(fd);
			return false;
		}
	}

	close(fd);

	_chat->AddAttachment(attachment);
	return true;
}
