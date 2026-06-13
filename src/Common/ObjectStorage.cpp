#include "ObjectStorage.hpp"

#include <unistd.h>
#include <fcntl.h>

#include "File.hpp"
#include "BinaryFile.hpp"
#include "Hex.hpp"
#include "../Crypto/Crypto.hpp"

#define OBJECT_PREFIX "objects"
#define REFS_PREFIX "refs"

ObjectStorage::ID::ID()
{
	memset(_data, 0, (int)ObjectStorage::Constants::IDSize);
}

ObjectStorage::ID::ID(const uint8_t *value)
{
	memcpy(_data, value, (int)ObjectStorage::Constants::IDSize);
}

ObjectStorage::ID::ID(const ID &id)
{
	memcpy(_data, id._data, (int)ObjectStorage::Constants::IDSize);
}

const uint8_t *ObjectStorage::ID::GetValue() const
{
	return _data;
}

void ObjectStorage::ID::SetValue(const uint8_t *value)
{
	memcpy(_data, value, (int)ObjectStorage::Constants::IDSize);
}

bool ObjectStorage::ID::IsZero() const
{
	for (int i = 0; i < (int)ObjectStorage::Constants::IDSize; i++) {
		if (_data[i]) {
			return false;
		}
	}

	return true;
}

ObjectStorage::ObjectStorage(String rootPath, EventDispatcher *dispatcher)
{
	_rootPath = rootPath;
	_dispatcher = dispatcher;
	_registered = false;

	_user = nullptr;

	_reader = nullptr;
	_writer = nullptr;
	_operationFirst = nullptr;
	_operationLast = nullptr;
	_operationFd = -1;

	if (!FileExists(rootPath)) {
		CreateDirectory(rootPath);
	}

	if (!FileExists(rootPath + "/" + OBJECT_PREFIX)) {
		CreateDirectory(rootPath + "/" + OBJECT_PREFIX);
	}

	if (!FileExists(rootPath + "/" + REFS_PREFIX)) {
		CreateDirectory(rootPath + "/" + REFS_PREFIX);
	}
}

ObjectStorage::~ObjectStorage()
{
	FinalizeOperations();
}

void ObjectStorage::SetUser(ObjectStorageUser *user)
{
	_user = user;
}

bool ObjectStorage::HasRef(String refName)
{
	String path = _rootPath + "/" + REFS_PREFIX + "/" + refName;
	return FileExists(path);
}

ObjectStorage::ID ObjectStorage::GetRef(String refName)
{
	String path = _rootPath + "/" + REFS_PREFIX + "/" + refName;

	BinaryFile file(path, false);
	uint8_t value[(int)Constants::IDSize];
	file.Read<uint8_t>(value, (int)Constants::IDSize, 0);

	return ID(value);
}

void ObjectStorage::SetRef(String refName, const ID &id)
{
	String path = _rootPath + "/" + REFS_PREFIX + "/" + refName;

	BinaryFile file(path, true);
	file.Write<uint8_t>(id.GetValue(), (int)Constants::IDSize, 0);
}

void ObjectStorage::DelRef(String refName)
{
	String path = _rootPath + "/" + REFS_PREFIX + "/" + refName;

	if (!FileExists(path)) {
		return;
	}

	int res = unlink(path.CStr());

	if (res == -1) {
		THROW("Failed to remove reference " + refName + ".");
	}
}

ObjectStorage::ID ObjectStorage::GetFreeID(const CowBuffer<uint8_t> object)
{
	ID id(GetHash(object, (int)Constants::IDSize).Pointer());

	if (!HasObject(id)) {
		return id;
	}

	CowBuffer<CowBuffer<uint8_t>> pair(2);
	pair[0] = object;
	pair[1] = CowBuffer<uint8_t>(8);

	for (;;) {
		GenerateRandomData(pair[1].Size(), pair[1].Pointer(), false);
		id.SetValue(GetHash(pair, (int)Constants::IDSize).Pointer());

		if (!HasObject(id)) {
			return id;
		}
	}
}

bool ObjectStorage::HasObject(const ID &id)
{
	String path = GetPathForID(id, false);
	return FileExists(path);
}

CowBuffer<uint8_t> ObjectStorage::ReadObject(const ID &id)
{
	String path = GetPathForID(id, false);

	BinaryFile file(path, false);
	CowBuffer<uint8_t> result(file.Size());
	file.Read<uint8_t>(result.Pointer(), result.Size(), 0);
	return result;
}

void ObjectStorage::WriteObject(const ID &id, CowBuffer<uint8_t> buffer)
{
	String path = GetPathForID(id, true);

	BinaryFile file(path, true, true);
	file.Write<uint8_t>(buffer.Pointer(), buffer.Size(), 0);
}

void ObjectStorage::UpdateObject(
	const ID &id,
	CowBuffer<uint8_t> buffer,
	uint64_t offset)
{
	String path = GetPathForID(id, true);

	BinaryFile file(path, true);
	file.Write<uint8_t>(buffer.Pointer(), buffer.Size(), offset);
}

void ObjectStorage::RequestObjectRead(const ID &id)
{
	OperationNode *node = new OperationNode(id);
	node->Type = OperationNode::Read;
	node->Next = nullptr;

	if (!_operationFirst) {
		_operationFirst = node;
		_operationLast = node;
	} else {
		_operationLast->Next = node;
		_operationLast = node;
	}

	StartOperation();
}

void ObjectStorage::RequestObjectWrite(
	const ID &id,
	const CowBuffer<uint8_t> buffer)
{
	OperationNode *node = new OperationNode(id);
	node->Type = OperationNode::Write;
	node->Next = nullptr;
	node->Data = buffer;

	if (!_operationFirst) {
		_operationFirst = node;
		_operationLast = node;
	} else {
		_operationLast->Next = node;
		_operationLast = node;
	}

	StartOperation();
}

void ObjectStorage::RequestObjectUpdate(
	const ID &id,
	const CowBuffer<uint8_t> buffer,
	uint64_t offset)
{
	OperationNode *node = new OperationNode(id);
	node->Type = OperationNode::Update;
	node->Next = nullptr;
	node->Data = buffer;
	node->UpdateOffset = offset;

	if (!_operationFirst) {
		_operationFirst = node;
		_operationLast = node;
	} else {
		_operationLast->Next = node;
		_operationLast = node;
	}

	StartOperation();
}

int ObjectStorage::GetDescriptor()
{
	return _operationFd;
}

bool ObjectStorage::RequestRead()
{
	return _reader;
}

bool ObjectStorage::RequestWrite()
{
	return _writer;
}

void ObjectStorage::ProcessRead()
{
	if (!_reader) {
		THROW("Reader is null.");
	}

	bool readSuccessful = _reader->Read();

	if (!readSuccessful) {
		delete _reader;
		_reader = nullptr;
		close(_operationFd);
		_operationFd = -1;
		THROW("Object storage reading error.");
	}

	bool readEnd = _reader->ReadingEnd();

	if (!readEnd) {
		return;
	}

	CowBuffer<uint8_t> buffer = _reader->GetBuffer();
	delete _reader;
	_reader = nullptr;
	close(_operationFd);
	_operationFd = -1;

	if (_user) {
		_user->ProcessRequestedObject(
			_operationFirst->Identifier,
			buffer);
	}

	OperationNode *tmp = _operationFirst;

	_operationFirst = _operationFirst->Next;

	if (!_operationFirst) {
		_operationLast = nullptr;
	}

	delete tmp;

	StartOperation();
}

void ObjectStorage::ProcessWrite()
{
	if (!_writer) {
		THROW("Writer is null.");
	}

	bool writeSuccessful = _writer->Write();

	if (!writeSuccessful) {
		delete _writer;
		_writer = nullptr;
		close(_operationFd);
		_operationFd = -1;
		THROW("Object storage writing error.");
	}

	bool writeEnd = _writer->WritingEnd();

	if (!writeEnd) {
		return;
	}

	delete _writer;
	_writer = nullptr;
	close(_operationFd);
	_operationFd = -1;

	if (_user) {
		_user->NotifyWriteCompleted(_operationFirst->Identifier);
	}

	OperationNode *tmp = _operationFirst;

	_operationFirst = _operationFirst->Next;

	if (!_operationFirst) {
		_operationLast = nullptr;
	}

	delete tmp;

	StartOperation();
}

void ObjectStorage::StartOperation()
{
	if (_reader || _writer) {
		return;
	}

	if (!_operationFirst) {
		if (_registered) {
			_dispatcher->UnregisterDescriptorProcessor(this);
			_registered = false;
		}

		return;
	}

	String path = GetPathForID(_operationFirst->Identifier, true);

	if (_operationFirst->Type == OperationNode::Read) {
		BinaryFile file(path, false);

		_operationFd = open(path.CStr(), O_RDONLY);

		if (_operationFd == -1) {
			THROW("Failed to open file for reading.");
		}

		_reader = new StreamReader(_operationFd, file.Size());
	} else if (_operationFirst->Type == OperationNode::Write) {
		_operationFd = open(
			path.CStr(),
			O_WRONLY | O_CREAT | O_TRUNC,
			0600);

		if (!_operationFd) {
			THROW("Failed to open file for writing.");
		}

		_writer = new StreamWriter(_operationFd, _operationFirst->Data);
	} else if (_operationFirst->Type == OperationNode::Update) {
		_operationFd = open(path.CStr(), O_WRONLY | O_CREAT, 0600);

		if (!_operationFd) {
			THROW("Failed to open file for writing.");
		}

		long res = lseek(
			_operationFd,
			_operationFirst->UpdateOffset,
			SEEK_SET);

		if (res == -1) {
			THROW("Failed to seek object file.");
		}

		_writer = new StreamWriter(_operationFd, _operationFirst->Data);
	}

	if (!_registered) {
		_dispatcher->RegisterDescriptorProcessor(this);
		_registered = true;
	}
}

void ObjectStorage::FinalizeOperations()
{
	_user = nullptr;

	while (_registered) {
		if (RequestRead()) {
			ProcessRead();
		}

		if (RequestWrite()) {
			ProcessWrite();
		}
	}
}

String ObjectStorage::GetPathForID(const ID &id, bool create)
{
	String idHex = DataToHex(id.GetValue(), (int)Constants::IDSize);

	String part1 = idHex.Substring(0, 3);
	String part2 = idHex.Substring(3, 3);
	String part3 = idHex.Substring(6, 3);

	if (create) {
		String path = _rootPath + "/" + OBJECT_PREFIX + "/" + part1;
		CreateDirectory(path);
		path += "/" + part2;
		CreateDirectory(path);
		path += "/" + part3;
		CreateDirectory(path);
	}

	return _rootPath + "/" + OBJECT_PREFIX + "/" +
		part1 + "/" + part2 + "/" + part3 + "/" + idHex;
}
