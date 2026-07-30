#ifndef _OBJECT_STORAGE_HPP
#define _OBJECT_STORAGE_HPP

#include "MyString.hpp"
#include "EventDispatcher.hpp"
#include "StreamReader.hpp"
#include "StreamWriter.hpp"

class ObjectStorageUser;

class ObjectStorage : public DescriptorEventProcessor
{
public:
	enum class Constants
	{
		IDSize = 32
	};

	class ID
	{
	public:
		ID();
		ID(const uint8_t *value);
		ID(const ID &id);

		const uint8_t *GetValuePointer() const;
		CowBuffer<uint8_t> GetValue() const;
		void GetValue(uint8_t *buffer) const;
		void SetValue(const uint8_t *value);

		bool IsZero() const;

		bool operator==(const ID &id) const;
		bool operator<(const ID &id) const;

	private:
		uint8_t _data[(int)Constants::IDSize];
	};

	ObjectStorage(String rootPath, EventDispatcher *dispatcher);
	~ObjectStorage();

	String GetPath();

	void SetUser(ObjectStorageUser *user);

	bool HasRef(String refName);
	ID GetRef(String refName);
	void SetRef(String refName, const ID &id);
	void DelRef(String refName);

	ID GetFreeID(const CowBuffer<uint8_t> object);

	bool HasObject(const ID &id);
	CowBuffer<uint8_t> ReadObject(const ID &id);
	CowBuffer<uint8_t> ReadObject(
		const ID &id,
		uint64_t offset,
		uint64_t length);
	void WriteObject(const ID &id, const CowBuffer<uint8_t> buffer);
	void UpdateObject(
		const ID &id,
		const CowBuffer<uint8_t> buffer,
		uint64_t offset);

	void RequestObjectRead(const ID &id);
	void RequestObjectWrite(const ID &id, const CowBuffer<uint8_t> buffer);
	void RequestObjectUpdate(
		const ID &id,
		const CowBuffer<uint8_t> buffer,
		uint64_t offset);

	int GetDescriptor() override;
	bool RequestRead() override;
	bool RequestWrite() override;
	void ProcessRead() override;
	void ProcessWrite() override;

private:
	String _rootPath;
	EventDispatcher *_dispatcher;
	bool _registered;

	StreamReader *_reader;
	StreamWriter *_writer;
	int _operationFd;

	ObjectStorageUser *_user;

	struct OperationNode
	{
		enum OperationType
		{
			Read,
			Write,
			Update
		};

		OperationNode *Next;

		OperationType Type;
		ID Identifier;
		CowBuffer<uint8_t> Data;
		uint64_t UpdateOffset;

		OperationNode(const ID &id) : Identifier(id)
		{ }
	};

	OperationNode *_operationFirst;
	OperationNode *_operationLast;

	void StartOperation();
	void FinalizeOperations();

	String GetPathForID(const ID &id, bool create);
};

class ObjectStorageUser
{
public:
	virtual ~ObjectStorageUser()
	{ }

	virtual void ProcessRequestedObject(
		const ObjectStorage::ID &id,
		const CowBuffer<uint8_t> buffer) = 0;
	virtual void NotifyWriteCompleted(const ObjectStorage::ID &id) = 0;
};

#endif
