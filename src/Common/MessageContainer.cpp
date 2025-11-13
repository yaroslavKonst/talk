#include "MessageContainer.hpp"

#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include "Hex.hpp"
#include "BinaryFile.hpp"
#include "../Crypto/CryptoDefinitions.hpp"

MessageContainer::MessageContainer(
	const uint8_t *key1,
	const uint8_t *key2)
{
	String k1h = DataToHex(key1, KEY_SIZE);
	String k2h = DataToHex(key2, KEY_SIZE);

	_root = k1h + "/" + k2h;

	CreateDirectory(k1h);
	CreateDirectory(_root);

	_dir = opendir(_root.CStr());

	if (!_dir) {
		THROW("Failed to open directory.");
	}
}

MessageContainer::~MessageContainer()
{
	closedir(_dir);
	_dir = nullptr;
}

void MessageContainer::AddMessage(CowBuffer<uint8_t> message)
{
	int64_t timestamp;
	memcpy(&timestamp, message.Pointer() + KEY_SIZE * 2, sizeof(timestamp));

	String timeString = ToString(timestamp);

	int suffix = 0;

	String entryPath = _root + "/" + timeString + "_" + ToString(suffix);

	while (FileExists(entryPath)) {
		++suffix;
		entryPath = _root + "/" + timeString + "_" + ToString(suffix);
	}

	BinaryFile file(entryPath, true);

	file.Write<uint8_t>(
		message.Pointer(),
		message.Size(),
		0);
}

CowBuffer<uint8_t> *MessageContainer::GetMessageRange(
	int64_t from,
	int64_t to,
	uint64_t &messageCount)
{
	Elem *first = nullptr;
	Elem *last = nullptr;

	rewinddir(_dir);

	messageCount = 0;

	struct dirent *dent;

	while ((dent = readdir(_dir)) != nullptr) {
		String name(dent->d_name);

		int partCount;
		String *parts = name.Split('_', false, partCount);

		if (partCount != 2) {
			THROW("Invalid message name.");
		}

		int64_t timestamp = atoll(parts[0].CStr());
		delete[] parts;

		if (timestamp >= from && timestamp <= to) {
			Elem *elem = new Elem;
			elem->Next = nullptr;
			elem->Name = dent->d_name;

			if (!first) {
				first = elem;
				last = elem;
			} else {
				last->Next = elem;
				last = elem;
			}

			++messageCount;
		}
	}

	if (!messageCount) {
		return nullptr;
	}

	CowBuffer<uint8_t> *result = new CowBuffer<uint8_t>[messageCount];
	uint64_t index = 0;

	while (first) {
		BinaryFile file(_root + "/" + first->Name, false);
		result[index] = CowBuffer<uint8_t>(file.Size());
		file.Read<uint8_t>(
			result[index].Pointer(),
			result[index].Size(),
			0);

		Elem *elem = first;
		first = first->Next;
		delete elem;
	}

	return result;
}

void MessageContainer::CreateDirectory(String path)
{
	int fd = open(path.CStr(), O_RDONLY | O_DIRECTORY);

	if (fd == -1) {
		if (errno != ENOENT) {
			THROW("Error on checking directory.");
		}

		fd = mkdir(path.CStr(), 0700);

		if (fd == -1) {
			THROW("Failed to create directory.");
		}
	} else {
		close(fd);
	}
}

bool MessageContainer::FileExists(String path)
{
	int fd = open(path.CStr(), O_RDONLY);

	if (fd == -1) {
		return false;
	}

	close(fd);
	return true;
}
