#include "IniFile.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <cstdio>

#include "Exception.hpp"

IniFile::IniFile(String path)
{
	_path = path;
	_data = nullptr;

	_modified = false;

	Load();
}

IniFile::~IniFile()
{
	if (_modified) {
		Save();
	}

	Free();
}

String IniFile::Get(String section, String key)
{
	Section *currentSection = _data;

	while (currentSection) {
		if (currentSection->Name == section) {
			Key *currentKey = currentSection->Keys;

			while (currentKey) {
				if (currentKey->Name == key) {
					return currentKey->Value;
				}

				currentKey = currentKey->Next;
			}
		}

		currentSection = currentSection->Next;
	}

	return String();
}

void IniFile::Set(String section, String key, String value)
{
	Section *currentSection = _data;

	while (currentSection) {
		if (currentSection->Name == section) {
			break;
		}

		currentSection = currentSection->Next;
	}

	if (!currentSection) {
		Section *newSection = new Section;

		if (!_data) {
			_data = newSection;
			currentSection = _data;
		} else {
			currentSection = _data;

			while (currentSection->Next) {
				currentSection = currentSection->Next;
			}

			currentSection->Next = newSection;
			currentSection = currentSection->Next;
		}

		currentSection->Next = nullptr;
		currentSection->Name = section;
		currentSection->Keys = nullptr;
	}

	Key *currentKey = currentSection->Keys;

	while (currentKey) {
		if (currentKey->Name == key) {
			break;
		}

		currentKey = currentKey->Next;
	}

	if (!currentKey) {
		if (!currentSection->Keys) {
			currentSection->Keys = new Key;
			currentKey = currentSection->Keys;
		} else {
			currentKey = currentSection->Keys;

			while (currentKey->Next) {
				currentKey = currentKey->Next;
			}

			currentKey->Next = new Key;
			currentKey = currentKey->Next;
		}

		currentKey->Next = nullptr;
		currentKey->Name = key;
	}

	currentKey->Value = value;

	_modified = true;
}

void IniFile::Clear()
{
	Free();
	_modified = true;
}

void IniFile::Load()
{
	if (_modified) {
		Save();
	}

	Free();

	_modified = false;

	int fd = open(_path.CStr(), O_RDONLY);

	if (fd == -1) {
		THROW(String("Ini: Failed to open ") + _path + ".");
	}

	int fileSize = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);

	if (fileSize <= 0) {
		close(fd);
		THROW(String("Ini: Failed to get size of ") + _path + ".");
	}

	char *fileBuffer = new char[fileSize + 1];
	int readBytes = read(fd, fileBuffer, fileSize);

	if (readBytes != fileSize) {
		close(fd);
		delete[] fileBuffer;
		THROW(String("Ini: Failed to read ") + _path + ".");
	}

	fileBuffer[fileSize] = 0;

	close(fd);

	String fileData(fileBuffer);
	delete[] fileBuffer;

	int lineCount;
	String *lines = fileData.Split('\n', true, lineCount);

	String currentSection;

	for (int i = 0; i < lineCount; i++) {
		String line = lines[i].Trim();

		if (line.Length() == 0) {
			continue;
		}

		// Comment.
		if (line.CStr()[0] == '#') {
			continue;
		}

		// Section.
		if (
			line.CStr()[0] == '[' &&
			line.CStr()[line.Length() - 1] == ']')
		{
			currentSection = line.Substring(1, line.Length() - 2);
			continue;
		}

		// Key
		int kvpLen;
		String *kvp = line.Split('=', false, kvpLen);

		if (kvpLen < 2) {
			printf(
				"Ini: %s: error in line %d.\n",
				_path.CStr(),
				i);
			continue;
		}

		String key = kvp[0].Trim();
		String value;

		for (int p = 1; p < kvpLen; p++) {
			value += kvp[p];
		}

		delete[] kvp;

		Set(currentSection, key, value.Trim());
	}

	delete[] lines;

	_modified = false;
}

void IniFile::Save()
{
	String fileData;
	bool lineWritten = false;

	Section *currentSection = _data;

	while (currentSection) {
		if (currentSection->Name.Length() > 0) {
			if (lineWritten) {
				fileData += '\n';
			}

			fileData += '[';
			fileData += currentSection->Name;
			fileData += "]\n";

			lineWritten = true;
		}

		Key *currentKey = currentSection->Keys;

		while (currentKey) {
			fileData += currentKey->Name;
			fileData += " = ";
			fileData += currentKey->Value;
			fileData += '\n';

			lineWritten = true;
		}
	}

	int fd = open(_path.CStr(), O_WRONLY | O_CREAT | O_TRUNC, 0666);

	if (fd == -1) {
		THROW(String("Ini: Failed to open ") + _path + " for writing.");
	}

	int res = write(fd, fileData.CStr(), fileData.Length());

	close(fd);

	if (res != fileData.Length()) {
		THROW(String("Ini: Failed to write ") + _path + ".");
	}
}

void IniFile::Free()
{
	Section *currentSection = _data;

	while (currentSection) {
		Key *currentKey = currentSection->Keys;

		while (currentKey) {
			Key *tmp = currentKey;
			currentKey = currentKey->Next;
			delete tmp;
		}

		Section *tmp = currentSection;
		currentSection = currentSection->Next;
		delete tmp;
	}

	_data = nullptr;
}
