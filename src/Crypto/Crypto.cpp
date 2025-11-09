#include "Crypto.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/random.h>

#include "../Common/Exception.hpp"

int64_t GetUnixTime()
{
	int64_t val = time(nullptr);

	if (val == -1) {
		THROW("Failed to get system time.");
	}

	return val;
}

static void Scramble(uint8_t *buffer, uint64_t size, uint8_t init)
{
	uint8_t val = init;
	uint64_t i;

	for (i = 0; i < size; i++) {
		buffer[i] = buffer[i] ^ val;
		val += 47;
	}
}

static void GenerateRandomData(
	uint64_t size,
	uint8_t *buffer,
	bool random = true)
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

static void UpdateNonce(uint8_t nonce[NONCE_SIZE])
{
	int64_t t = GetUnixTime();

	memcpy(nonce, &t, sizeof(t));

	for (int i = NONCE_SIZE - 1; i >= (int)sizeof(t); i--) {
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
	int64_t prevT;
	int64_t t;
	int equal = 1;
	int i;

	for (i = 0; i < NONCE_SIZE; i++) {
		if (prevNonce[i] != nonce[i]) {
			equal = 0;
		}
	}

	if (equal) {
		return false;
	}

	memcpy(&prevT, prevNonce, sizeof(t));
	memcpy(&t, nonce, sizeof(t));

	if (t < prevT) {
		return false;
	}

	return true;
}

void InitNonce(uint8_t nonce[NONCE_SIZE])
{
	memset(nonce, 0, NONCE_SIZE);
	int64_t t = GetUnixTime();
	memcpy(nonce, &t, sizeof(t));
}

void InitStream(
	EncryptedStream &stream,
	const uint8_t key[KEY_SIZE])
{
	memcpy(stream.Key, key, KEY_SIZE);
	InitNonce(stream.Nonce);
}

void InitStream(
	EncryptedStream &stream,
	const uint8_t key[KEY_SIZE],
	const uint8_t nonce[NONCE_SIZE])
{
	memcpy(stream.Key, key, KEY_SIZE);
	memcpy(stream.Nonce, nonce, NONCE_SIZE);
}

/*
Encrypted block structure.
| scrambler init |               data             |
                 | mac | nonce |      message     |
*/
CowBuffer<uint8_t> Encrypt(
	const CowBuffer<uint8_t> plaintext,
	EncryptedStream &stream)
{
	CowBuffer<uint8_t> result(
		1 + MAC_SIZE + NONCE_SIZE + plaintext.Size());

	uint8_t *scramblerInit = result.Pointer();
	uint8_t *mac = result.Pointer() + 1;
	uint8_t *nonce = result.Pointer() + 1 + MAC_SIZE;
	uint8_t *message = result.Pointer() + 1 + MAC_SIZE + NONCE_SIZE;

	UpdateNonce(stream.Nonce);

	memcpy(nonce, stream.Nonce, NONCE_SIZE);

	crypto_aead_lock(
		message,
		mac,
		stream.Key,
		nonce,
		nullptr,
		0,
		plaintext.Pointer(),
		plaintext.Size());

	Scramble(result.Pointer() + 1, result.Size() - 1, scramblerInit[0]);
	return result;
}

CowBuffer<uint8_t> Decrypt(
	const CowBuffer<uint8_t> cyphertext,
	EncryptedStream &stream)
{
	if (cyphertext.Size() <= 1 + MAC_SIZE + NONCE_SIZE) {
		return CowBuffer<uint8_t>();
	}

	CowBuffer<uint8_t> workplace = cyphertext;

	uint8_t *scramblerInit = workplace.Pointer();
	uint8_t *mac = workplace.Pointer() + 1;
	uint8_t *nonce = workplace.Pointer() + 1 + MAC_SIZE;
	uint8_t *message = workplace.Pointer() + 1 + MAC_SIZE + NONCE_SIZE;

	Scramble(
		workplace.Pointer() + 1,
		workplace.Size() - 1,
		scramblerInit[0]);

	int success = VerifyNonce(stream.Nonce, nonce);

	if (!success) {
		return CowBuffer<uint8_t>();
	}

	CowBuffer<uint8_t> result(
		workplace.Size() - (1 + MAC_SIZE + NONCE_SIZE));

	success = crypto_aead_unlock(
		result.Pointer(),
		mac,
		stream.Key,
		nonce,
		nullptr,
		0,
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
	const uint8_t key[SIGNATURE_PRIVATE_KEY_SIZE],
	uint8_t signature[SIGNATURE_SIZE])
{
	crypto_eddsa_sign(signature, key, data.Pointer(), data.Size());
}

bool Verify(
	const CowBuffer<uint8_t> data,
	const uint8_t key[SIGNATURE_PUBLIC_KEY_SIZE],
	const uint8_t signature[SIGNATURE_SIZE])
{
	int res = crypto_eddsa_check(
		signature,
		key,
		data.Pointer(),
		data.Size());

	return res == 0;
}

void DeriveKey(
	const char *password,
	const uint8_t salt[SALT_SIZE],
	uint8_t key[KEY_SIZE])
{
	crypto_argon2_config config;
	config.algorithm = CRYPTO_ARGON2_I;
	config.nb_blocks = 100000;
	config.nb_passes = 3;
	config.nb_lanes = 1;

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

	char *workArea = new char[config.nb_blocks * 1024];
	crypto_argon2(key, KEY_SIZE, workArea, config, inputs, extras);
	delete[] workArea;
}

void GeneratePublicKey(
	const uint8_t privateKey[KEY_SIZE],
	uint8_t publicKey[KEY_SIZE])
{
	crypto_x25519_public_key(publicKey, privateKey);
}

void GenerateSessionKeys(
	const uint8_t privateKey[KEY_SIZE],
	const uint8_t publicKey[KEY_SIZE],
	const uint8_t peerPublicKey[KEY_SIZE],
	int64_t addition,
	uint8_t sessionKey1[KEY_SIZE],
	uint8_t sessionKey2[KEY_SIZE],
	bool invert)
{
	uint8_t sharedSecret[KEY_SIZE];
	crypto_x25519(sharedSecret, privateKey, peerPublicKey);

	uint8_t sharedKeys[KEY_SIZE * 2];
	crypto_blake2b_ctx ctx;
	crypto_blake2b_init(&ctx, KEY_SIZE * 2);
	crypto_blake2b_update(&ctx, sharedSecret, KEY_SIZE);

	if (!invert) {
		crypto_blake2b_update(&ctx, publicKey, KEY_SIZE);
	}

	crypto_blake2b_update(&ctx, peerPublicKey, KEY_SIZE);

	if (invert) {
		crypto_blake2b_update(&ctx, publicKey, KEY_SIZE);
	}

	crypto_blake2b_update(&ctx, (uint8_t*)&addition, sizeof(addition));
	crypto_blake2b_final(&ctx, sharedKeys);

	memcpy(sessionKey1, sharedKeys, KEY_SIZE);
	memcpy(sessionKey2, sharedKeys + KEY_SIZE, KEY_SIZE);
	crypto_wipe(sharedSecret, KEY_SIZE);
	crypto_wipe(sharedKeys, KEY_SIZE * 2);
}

void GenerateSignature(
	uint8_t seed[KEY_SIZE],
	uint8_t signaturePrivateKey[SIGNATURE_PRIVATE_KEY_SIZE],
	uint8_t signaturePublicKey[SIGNATURE_PUBLIC_KEY_SIZE])
{
	crypto_eddsa_key_pair(signaturePrivateKey, signaturePublicKey, seed);
}

void GetSalt(String file, uint8_t salt[SALT_SIZE])
{
	int fd = open(file.CStr(), O_RDONLY);

	if (fd == -1) {
		GenerateRandomData(SALT_SIZE, salt, true);

		fd = open(file.CStr(), O_WRONLY | O_CREAT, 0600);

		if (fd == -1) {
			THROW("Failed to open salt file for writing.");
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
