#ifndef _CRYPTO_HPP
#define _CRYPTO_HPP

#include <cstring>

#include "../Common/CowBuffer.hpp"
#include "../Common/MyString.hpp"
#include "../ThirdParty/monocypher.h"
#include "CryptoDefinitions.hpp"

namespace Crypto
{
	struct KeyContainerBase
	{
		int32_t KeyScheme;
		int32_t KeyType;

		virtual ~KeyContainerBase()
		{ }
	};

	CowBuffer<uint8_t> ApplyScrambler(CowBuffer<uint8_t> data);
	CowBuffer<uint8_t> RemoveScrambler(CowBuffer<uint8_t> data);
	uint8_t ApplyScrambler(
		uint8_t *buffer,
		uint64_t size,
		uint8_t init);

	void GenerateRandomData(
		uint64_t size,
		uint8_t *buffer,
		bool random = true);

	CowBuffer<uint8_t> GetHash(
		const CowBuffer<uint8_t> buffer,
		int hashSize);

	CowBuffer<uint8_t> GetHash(
		const CowBuffer<CowBuffer<uint8_t>> buffers,
		int hashSize);

	namespace X25519
	{
		template<int KeySize, int KeyId>
		struct KeyContainerTemplate : public KeyContainerBase
		{
			uint8_t Key[KeySize];

			KeyContainerTemplate()
			{
				InitID();
			}

			KeyContainerTemplate(
				const KeyContainerTemplate<KeySize, KeyId> &c)
			{
				InitID();
				memcpy(Key, c.Key, KeySize);
			}

			~KeyContainerTemplate()
			{
				crypto_wipe(Key, KeySize);
			}

			KeyContainerTemplate<KeySize, KeyId> &operator=(
				const KeyContainerTemplate<KeySize, KeyId> &c)
			{
				memcpy(Key, c.Key, KeySize);
				return *this;
			}

			KeyContainerTemplate<KeySize, KeyId> &operator=(
				const uint8_t *data)
			{
				memcpy(Key, data, KeySize);
				return *this;
			}

			void InitID()
			{
				KeyScheme = SCHEME_ID;
				KeyType = KeyId;
			}

			bool operator==(
				const KeyContainerTemplate<KeySize, KeyId> &c)
				const
			{
				bool eq = true;

				for (int i = 0; i < KeySize; i++) {
					if (Key[i] != c.Key[i]) {
						eq = false;
					}
				}

				return eq;
			}
		};

		typedef KeyContainerTemplate<KEY_SIZE, SYMMETRIC_KEY_ID>
			SymmetricKeyContainer;

		typedef KeyContainerTemplate<KEY_SIZE, PRIVATE_KEY_ID>
			PrivateKeyContainer;

		typedef KeyContainerTemplate<KEY_SIZE, PUBLIC_KEY_ID>
			PublicKeyContainer;

		typedef KeyContainerTemplate<
				SIGNATURE_PRIVATE_KEY_SIZE,
				SIGNATURE_PRIVATE_KEY_ID>
			SignaturePrivateKeyContainer;

		typedef KeyContainerTemplate<
				SIGNATURE_PUBLIC_KEY_SIZE,
				SIGNATURE_PUBLIC_KEY_ID>
			SignaturePublicKeyContainer;

		struct EncryptedStream
		{
			SymmetricKeyContainer Key;
			uint8_t Nonce[NONCE_SIZE];

			EncryptedStream()
			{ }

			EncryptedStream(const EncryptedStream &s)
			{
				Key = s.Key;
				memcpy(Nonce, s.Nonce, NONCE_SIZE);
			}

			~EncryptedStream()
			{
				crypto_wipe(Nonce, NONCE_SIZE);
			}
		};

		void InitStream(
			EncryptedStream &stream,
			const SymmetricKeyContainer &key);
		void InitStream(
			EncryptedStream &stream,
			const SymmetricKeyContainer &key,
			const uint8_t nonce[NONCE_SIZE]);

		void InitNonce(uint8_t nonce[NONCE_SIZE]);

		CowBuffer<uint8_t> Encrypt(
			const CowBuffer<uint8_t> plaintext,
			EncryptedStream &stream,
			const uint8_t *addData = nullptr,
			uint64_t addSize = 0);

		CowBuffer<uint8_t> Decrypt(
			const CowBuffer<uint8_t> cyphertext,
			struct EncryptedStream &stream,
			const uint8_t *addData = nullptr,
			uint64_t addSize = 0);

		void Sign(
			const CowBuffer<uint8_t> data,
			const SignaturePrivateKeyContainer &key,
			uint8_t signature[SIGNATURE_SIZE]);

		bool Verify(
			const CowBuffer<uint8_t> data,
			const SignaturePublicKeyContainer &key,
			const uint8_t signature[SIGNATURE_SIZE]);

		void DeriveKey(
			const char *password,
			const uint8_t salt[SALT_SIZE],
			PrivateKeyContainer &key);

		void GeneratePublicKey(
			const PrivateKeyContainer &privateKey,
			PublicKeyContainer &publicKey);

		void GenerateSessionKeys(
			const PrivateKeyContainer &privateKey,
			const PublicKeyContainer &publicKey,
			const PublicKeyContainer &peerPublicKey,
			int64_t addition,
			SymmetricKeyContainer &sessionKey1,
			SymmetricKeyContainer &sessionKey2,
			bool invert = false);

		void GenerateSignature(
			uint8_t seed[KEY_SIZE],
			SignaturePrivateKeyContainer &signaturePrivateKey,
			SignaturePublicKeyContainer signaturePublicKey);

		void GetSalt(String file, uint8_t salt[SALT_SIZE]);

		class CryptoStreamReader
		{
		public:
			~CryptoStreamReader();

			bool Init(
				EncryptedStream *ES,
				const uint8_t nonce[NONCE_SIZE]);

			CowBuffer<uint8_t> Decrypt(
				const CowBuffer<uint8_t> cyphertext,
				const CowBuffer<uint8_t> add);

		private:
			crypto_aead_ctx _ctx;
		};

		class CryptoStreamWriter
		{
		public:
			~CryptoStreamWriter();

			void Init(EncryptedStream *ES);

			CowBuffer<uint8_t> Encrypt(
				const CowBuffer<uint8_t> plaintext,
				const CowBuffer<uint8_t> add);

		private:
			crypto_aead_ctx _ctx;
		};
	}
}

#endif
