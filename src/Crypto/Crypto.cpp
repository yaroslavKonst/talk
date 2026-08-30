#include "Crypto.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <sys/random.h>
#include <cstring>

#include "../Common/UnixTime.hpp"
#include "../Common/Exception.hpp"
#include "../Common/FileAccess.hpp"

namespace Crypto
{
	static uint8_t Gen(uint8_t val)
	{
		if (val == 0) {
			++val;
		}

		uint8_t bit = ((val >> 6) ^ (val >> 5) ^ (val >> 4) ^ val) & 1;
		return (val >> 1) | (bit << 7);
	}

	uint8_t ApplyScrambler(uint8_t *buffer, uint64_t size, uint8_t init)
	{
		uint8_t val = init;

		for (uint64_t i = 0; i < size; i++) {
			buffer[i] = buffer[i] ^ val;
			val = Gen(val);
		}

		return val;
	}

	CowBuffer<uint8_t> ApplyScrambler(CowBuffer<uint8_t> data)
	{
		CowBuffer<uint8_t> result(data.Size() + 1);

		if (data.Size()) {
			memcpy(
				result.Pointer() + 1,
				data.Pointer(),
				data.Size());
		}

		GenerateRandomData(1, result.Pointer(), false);

		if (data.Size()) {
			ApplyScrambler(
				result.Pointer() + 1,
				result.Size() - 1,
				result.Pointer()[0]);
		}

		return result;
	}

	CowBuffer<uint8_t> RemoveScrambler(CowBuffer<uint8_t> data)
	{
		if (data.Size() <= 1) {
			return CowBuffer<uint8_t>();
		}

		ApplyScrambler(
			data.Pointer() + 1,
			data.Size() - 1,
			data.Pointer()[0]);

		return data.Slice(1, data.Size() - 1);
	}

	void GenerateRandomData(
		uint64_t size,
		uint8_t *buffer,
		bool random)
	{
		uint64_t generatedBytes = 0;

		while (generatedBytes < size) {
			int res;

			res = getrandom(
				buffer + generatedBytes,
				size - generatedBytes,
				random ? GRND_RANDOM : 0);

			if (res == -1) {
				THROW("Failed to get random data.");
			}

			generatedBytes += res;
		}
	}

	CowBuffer<uint8_t> GetHash(
		const CowBuffer<uint8_t> buffer,
		int hashSize)
	{
		CowBuffer<uint8_t> hash(hashSize);

		crypto_blake2b(
			hash.Pointer(),
			hashSize,
			buffer.Pointer(),
			buffer.Size());

		return hash;
	}

	CowBuffer<uint8_t> GetHash(
		const CowBuffer<CowBuffer<uint8_t>> buffers,
		int hashSize)
	{
		CowBuffer<uint8_t> hash(hashSize);

		crypto_blake2b_ctx ctx;
		crypto_blake2b_init(&ctx, hashSize);

		for (uint64_t i = 0; i < buffers.Size(); i++) {
			const CowBuffer<uint8_t> buffer = buffers[i];
			crypto_blake2b_update(
				&ctx,
				buffer.Pointer(),
				buffer.Size());
		}

		crypto_blake2b_final(&ctx, hash.Pointer());
		return hash;
	}

	namespace X25519
	{
		static void UpdateNonce(uint8_t nonce[NONCE_SIZE])
		{
			for (int i = NONCE_SIZE - 1; i >= 0; i--) {
				if (nonce[i] < 255) {
					nonce[i] += 1;
					break;
				} else {
					nonce[i] = 0;
				}
			}
		}

		static bool VerifyNonce(
			const uint8_t prevNonce[NONCE_SIZE],
			const uint8_t nonce[NONCE_SIZE])
		{
			for (int i = 0; i < NONCE_SIZE; i++) {
				if (prevNonce[i] < nonce[i]) {
					return true;
				} else if (prevNonce[i] > nonce[i]) {
					return false;
				}
			}

			return false;
		}

		void InitNonce(uint8_t nonce[NONCE_SIZE])
		{
			memset(nonce, 0, 2);
			int offset = 2;

			GenerateRandomData(
				NONCE_SIZE - offset,
				nonce + offset,
				false);
		}

		void InitStream(
			EncryptedStream &stream,
			const SymmetricKeyContainer &key)
		{
			stream.Key = key;
			InitNonce(stream.Nonce);
		}

		void InitStream(
			EncryptedStream &stream,
			const SymmetricKeyContainer &key,
			const uint8_t nonce[NONCE_SIZE])
		{
			stream.Key = key;
			memcpy(stream.Nonce, nonce, NONCE_SIZE);
		}

		CowBuffer<uint8_t> Encrypt(
			const CowBuffer<uint8_t> plaintext,
			EncryptedStream &stream,
			const CowBuffer<uint8_t> addData)
		{
			CowBuffer<uint8_t> result(
				MAC_SIZE + NONCE_SIZE + plaintext.Size());

			uint8_t *mac = result.Pointer();
			uint8_t *nonce = result.Pointer() + MAC_SIZE;
			uint8_t *message =
				result.Pointer() + MAC_SIZE + NONCE_SIZE;

			UpdateNonce(stream.Nonce);

			memcpy(nonce, stream.Nonce, NONCE_SIZE);

			crypto_aead_lock(
				message,
				mac,
				stream.Key.Key,
				nonce,
				addData.Pointer(),
				addData.Size(),
				plaintext.Pointer(),
				plaintext.Size());

			return result;
		}

		CowBuffer<uint8_t> Decrypt(
			const CowBuffer<uint8_t> cyphertext,
			EncryptedStream &stream,
			const CowBuffer<uint8_t> addData)
		{
			if (cyphertext.Size() <= MAC_SIZE + NONCE_SIZE) {
				return CowBuffer<uint8_t>();
			}

			const uint8_t *mac = cyphertext.Pointer();
			const uint8_t *nonce = cyphertext.Pointer(MAC_SIZE);
			const uint8_t *message =
				cyphertext.Pointer(MAC_SIZE + NONCE_SIZE);

			int success = VerifyNonce(stream.Nonce, nonce);

			if (!success) {
				return CowBuffer<uint8_t>();
			}

			CowBuffer<uint8_t> result(
				cyphertext.Size() - (MAC_SIZE + NONCE_SIZE));

			success = crypto_aead_unlock(
				result.Pointer(),
				mac,
				stream.Key.Key,
				nonce,
				addData.Pointer(),
				addData.Size(),
				message,
				result.Size());

			if (success == -1) {
				return CowBuffer<uint8_t>();
			}

			memcpy(stream.Nonce, nonce, NONCE_SIZE);
			return result;
		}

		void Sign(
			const CowBuffer<uint8_t> data,
			const SignaturePrivateKeyContainer &key,
			uint8_t signature[SIGNATURE_SIZE])
		{
			crypto_eddsa_sign(
				signature,
				key.Key,
				data.Pointer(),
				data.Size());
		}

		bool Verify(
			const CowBuffer<uint8_t> data,
			const SignaturePublicKeyContainer &key,
			const uint8_t signature[SIGNATURE_SIZE])
		{
			int res = crypto_eddsa_check(
				signature,
				key.Key,
				data.Pointer(),
				data.Size());

			return res == 0;
		}

		void DeriveKey(
			const char *password,
			const uint8_t salt[SALT_SIZE],
			PrivateKeyContainer &key)
		{
			crypto_argon2_config config;
			config.algorithm = CRYPTO_ARGON2_I;
			config.nb_blocks = 100000;
			config.nb_passes = 3;
			config.nb_lanes = 1;

			int blockSize = 1024;

			crypto_argon2_inputs inputs;
			inputs.pass = (const uint8_t*)password;
			inputs.salt = salt;
			inputs.pass_size = strlen(password);
			inputs.salt_size = SALT_SIZE;

			crypto_argon2_extras extras;
			extras.key = 0;
			extras.ad = 0;
			extras.key_size = 0;
			extras.ad_size = 0;

			char *workArea = new char[config.nb_blocks * blockSize];
			crypto_argon2(
				key.Key,
				KEY_SIZE,
				workArea,
				config,
				inputs,
				extras);

			delete[] workArea;
		}

		void GeneratePublicKey(
			const PrivateKeyContainer &privateKey,
			PublicKeyContainer &publicKey)
		{
			crypto_x25519_public_key(publicKey.Key, privateKey.Key);
		}

		bool GenerateSessionKeys(
			const PrivateKeyContainer &privateKey,
			const PublicKeyContainer &publicKey,
			const PublicKeyContainer &peerPublicKey,
			const CowBuffer<uint8_t> addition,
			SymmetricKeyContainer &sessionKey1,
			SymmetricKeyContainer &sessionKey2,
			bool invert)
		{
			uint8_t sharedSecret[KEY_SIZE];
			crypto_x25519(
				sharedSecret,
				privateKey.Key,
				peerPublicKey.Key);

			uint8_t zeroBuffer[KEY_SIZE];
			memset(zeroBuffer, 0, KEY_SIZE);

			if (!crypto_verify32(sharedSecret, zeroBuffer)) {
				return false;
			}

			uint8_t sharedKeys[KEY_SIZE * 2];
			crypto_blake2b_ctx ctx;
			crypto_blake2b_init(&ctx, KEY_SIZE * 2);
			crypto_blake2b_update(&ctx, sharedSecret, KEY_SIZE);

			if (!invert) {
				crypto_blake2b_update(
					&ctx,
					publicKey.Key,
					KEY_SIZE);
			}

			crypto_blake2b_update(
				&ctx,
				peerPublicKey.Key,
				KEY_SIZE);

			if (invert) {
				crypto_blake2b_update(
					&ctx,
					publicKey.Key,
					KEY_SIZE);
			}

			if (addition.Size() > 0) {
				crypto_blake2b_update(
					&ctx,
					addition.Pointer(),
					addition.Size());
			}

			crypto_blake2b_final(&ctx, sharedKeys);

			memcpy(sessionKey1.Key, sharedKeys, KEY_SIZE);
			memcpy(
				sessionKey2.Key,
				sharedKeys + KEY_SIZE,
				KEY_SIZE);

			crypto_wipe(sharedSecret, KEY_SIZE);
			crypto_wipe(sharedKeys, KEY_SIZE * 2);

			return true;
		}

		void GenerateEphemeralKeyPair(
			PrivateKeyContainer &privateKey,
			PublicKeyContainer &publicKey)
		{
			CowBuffer<uint8_t> sourceData(512);

			Crypto::GenerateRandomData(
				sourceData.Size(),
				sourceData.Pointer(),
				false);

			CowBuffer<uint8_t> hash = Crypto::GetHash(
				sourceData,
				Crypto::X25519::KEY_SIZE);

			privateKey = hash.Pointer();

			memset(hash.Pointer(), 0, hash.Size());
			memset(sourceData.Pointer(), 0, sourceData.Size());

			Crypto::X25519::GeneratePublicKey(
				privateKey,
				publicKey);
		}

		void GenerateSignature(
			uint8_t seed[KEY_SIZE],
			SignaturePrivateKeyContainer &signaturePrivateKey,
			SignaturePublicKeyContainer &signaturePublicKey)
		{
			crypto_eddsa_key_pair(
				signaturePrivateKey.Key,
				signaturePublicKey.Key,
				seed);
		}

		void GetSalt(String file, uint8_t salt[SALT_SIZE])
		{
			int fd = open(file.CStr(), O_RDONLY);

			if (fd == -1) {
				GenerateRandomData(SALT_SIZE, salt, true);

				fd = open(
					file.CStr(),
					O_WRONLY | O_CREAT,
					FileAccessConstants::FileAccessRights);

				if (fd == -1) {
					THROW("Failed to open salt file for "
						"writing.");
				}

				int res = write(fd, salt, SALT_SIZE);

				if (res != SALT_SIZE) {
					close(fd);
					THROW("Failed to write salt file.");
				}
			} else {
				int res = read(fd, salt, SALT_SIZE);

				if (res != SALT_SIZE) {
					close(fd);
					THROW("Failed to read salt from file.");
				}
			}

			close(fd);
		}

		// Stream reader.
		CryptoStreamReader::~CryptoStreamReader()
		{
			crypto_wipe(&_ctx, sizeof(_ctx));
		}

		bool CryptoStreamReader::Init(
			EncryptedStream *ES,
			const uint8_t nonce[NONCE_SIZE])
		{
			bool success = VerifyNonce(ES->Nonce, nonce);

			if (!success) {
				return false;
			}

			memcpy(ES->Nonce, nonce, NONCE_SIZE);
			crypto_aead_init_x(&_ctx, ES->Key.Key, nonce);
			return true;
		}

		CowBuffer<uint8_t> CryptoStreamReader::Decrypt(
			const CowBuffer<uint8_t> cyphertext,
			const CowBuffer<uint8_t> add)
		{
			if (cyphertext.Size() <= MAC_SIZE) {
				return CowBuffer<uint8_t>();
			}

			CowBuffer<uint8_t> result(cyphertext.Size() - MAC_SIZE);

			int error = crypto_aead_read(
				&_ctx,
				result.Pointer(),
				cyphertext.Pointer(),
				add.Size() ? add.Pointer() : nullptr,
				add.Size(),
				cyphertext.Pointer(MAC_SIZE),
				result.Size());

			if (error) {
				return CowBuffer<uint8_t>();
			}

			return result;
		}

		// Stream writer.
		CryptoStreamWriter::~CryptoStreamWriter()
		{
			crypto_wipe(&_ctx, sizeof(_ctx));
		}

		void CryptoStreamWriter::Init(EncryptedStream *ES)
		{
			crypto_aead_init_x(&_ctx, ES->Key.Key, ES->Nonce);
		}

		CowBuffer<uint8_t> CryptoStreamWriter::Encrypt(
			const CowBuffer<uint8_t> plaintext,
			const CowBuffer<uint8_t> add)
		{
			CowBuffer<uint8_t> result(plaintext.Size() + MAC_SIZE);

			crypto_aead_write(
				&_ctx,
				result.Pointer(MAC_SIZE),
				result.Pointer(),
				add.Size() ? add.Pointer() : nullptr,
				add.Size(),
				plaintext.Pointer(),
				plaintext.Size());

			return result;
		}
	}
}
