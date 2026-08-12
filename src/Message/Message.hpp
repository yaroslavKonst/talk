#ifndef _MESSAGE_HPP
#define _MESSAGE_HPP

#include "../Common/CowBuffer.hpp"
#include "../Common/MyString.hpp"
#include "../Common/ObjectStorage.hpp"
#include "../Crypto/Crypto.hpp"

namespace Message
{
	enum class Type : uint8_t
	{
		PointToPoint = 1,
		Group = 2
	};

	Type GetMessageType(const CowBuffer<uint8_t> message);

	enum class Attribute : int32_t
	{
		// Direction.
		Inbound = 0x1,

		// Attributes for all directions.
		Unread = 0x2,

		// Outbound attributes.
		Local = 0x4,
		InProgress = 0x8,
		ConnectionFailure = 0x10,
		Rejected = 0x20,
		WrongDestinationUser = 0x40,
		WrongDestinationKey = 0x80,
		InvalidHeader = 0x100,
		MessageTooBig = 0x200,
		BannedSender = 0x400,
		BannedSenderKey = 0x800,
		Duplicate = 0x1000
	};

	namespace AttributeAction
	{
		bool Has(Attribute attrs, Attribute flag);
		Attribute Set(Attribute attrs, Attribute flag);
		Attribute Clear(Attribute attrs, Attribute flag);
	}

	enum class ContentsEntryType : uint8_t
	{
		Text = 0,
		Attachment = 1,
		Key = 2,
		Empty = 3,
		Unsupported = 255
	};

	struct ContentsEntry
	{
		virtual ~ContentsEntry()
		{ }

		ContentsEntryType Type;
	};

	struct ContentsEntryText : public ContentsEntry
	{
		String Text;
	};

	struct ContentsEntryAttachment : public ContentsEntry
	{
		String AttachmentName;
		CowBuffer<uint8_t> Attachment;
	};

	struct ContentsEntryKey : public ContentsEntry
	{
		String UserName;
		int32_t KeyType;
		Crypto::X25519::PublicKeyContainer Key;
	};

	struct ContentsEntryUnsupported : public ContentsEntry
	{
		uint8_t EntryTypeID;
	};

	struct Contents
	{
		~Contents();

		void Clear();

		CowBuffer<ContentsEntry*> Entries;
	};

	bool ParseContents(
		const CowBuffer<uint8_t> message,
		Contents &contents);

	CowBuffer<uint8_t> BuildContents(
		const Contents &contents,
		uint64_t emptySize);

	bool VerifyFullUserName(String name);
	bool VerifyFullGroupName(String name);
	bool VerifyFullHostName(String name);

	bool SplitFullUserName(
		String fullName,
		String &userName,
		String &hostName);

	// Accepts user and group names. Does less strict checks than
	// VerifyFull*Name functions.
	bool ExtractServerDataFromFullName(
		String fullName,
		String &hostName,
		String &serviceName);

	namespace X25519
	{
		struct HeaderPointToPoint
		{
			String Source;
			Crypto::X25519::PublicKeyContainer SourceKey;

			String Destination;
			Crypto::X25519::PublicKeyContainer DestinationKey;

			int64_t Timestamp;
			int32_t Index;

			ObjectStorage::ID ThreadID;

			uint64_t MessageSize;
			uint8_t Nonce[Crypto::X25519::NONCE_SIZE];

			uint64_t HeaderSize;
		};

		bool ParseHeader(
			const CowBuffer<uint8_t> message,
			HeaderPointToPoint &header);
		CowBuffer<uint8_t> BuildHeader(
			const HeaderPointToPoint &header);
		void WriteHeaderSize(HeaderPointToPoint &header);

		enum
		{
			GroupKeyEntrySize =
				Crypto::X25519::KEY_SIZE +
				Crypto::X25519::CRYPTO_HEADER_SIZE +
				Crypto::X25519::KEY_SIZE
		};

		struct GroupKeyEntry
		{
			Crypto::X25519::PublicKeyContainer DestinationKey;
			uint8_t MAC[Crypto::X25519::MAC_SIZE];
			uint8_t Nonce[Crypto::X25519::NONCE_SIZE];
			uint8_t EncryptedKey[Crypto::X25519::KEY_SIZE];
		};

		struct HeaderGroup
		{
			String Source;
			Crypto::X25519::PublicKeyContainer SourceKey;

			String GroupName;
			CowBuffer<GroupKeyEntry> GroupKeys;

			int64_t Timestamp;
			int32_t Index;

			ObjectStorage::ID ThreadID;

			uint64_t MessageSize;
			uint8_t Nonce[Crypto::X25519::NONCE_SIZE];

			uint64_t HeaderSize;
		};

		bool ParseHeader(
			const CowBuffer<uint8_t> message,
			HeaderGroup &result);
		CowBuffer<uint8_t> BuildHeader(const HeaderGroup &header);
		void WriteHeaderSize(HeaderGroup &header);
	}
};

#endif
