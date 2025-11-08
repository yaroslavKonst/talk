#ifndef _CRYPTO_HPP
#define _CRYPTO_HPP

#include "../Common/CowBuffer.hpp"
#include "../Common/MyString.hpp"
#include "../ThirdParty/monocypher.h"

#define KEY_SIZE 32
#define SIGNATURE_PRIVATE_KEY_SIZE 64
#define SIGNATURE_PUBLIC_KEY_SIZE 32
#define SIGNATURE_SIZE 64
#define NONCE_SIZE 24
#define MAC_SIZE 16
#define SALT_SIZE 16

struct EncryptedStream
{
	uint8_t Key[KEY_SIZE];
	uint8_t Nonce[NONCE_SIZE];
};

int64_t GetUnixTime();

void InitStream(EncryptedStream &stream, const uint8_t key[KEY_SIZE]);
void InitStream(
	EncryptedStream &stream,
	const uint8_t key[KEY_SIZE],
	const uint8_t nonce[NONCE_SIZE]);

void InitNonce(uint8_t nonce[NONCE_SIZE]);

CowBuffer<uint8_t> Encrypt(
	const CowBuffer<uint8_t> plaintext,
	EncryptedStream &stream);

CowBuffer<uint8_t> Decrypt(
	const CowBuffer<uint8_t> cyphertext,
	struct EncryptedStream &stream);

void Sign(
	const CowBuffer<uint8_t> data,
	const uint8_t key[SIGNATURE_PRIVATE_KEY_SIZE],
	uint8_t signature[SIGNATURE_SIZE]);

bool Verify(
	const CowBuffer<uint8_t> data,
	const uint8_t key[SIGNATURE_PUBLIC_KEY_SIZE],
	const uint8_t signature[SIGNATURE_SIZE]);

void DeriveKey(
	const char *password,
	const uint8_t salt[SALT_SIZE],
	uint8_t key[KEY_SIZE]);

void GeneratePublicKey(
	const uint8_t privateKey[KEY_SIZE],
	uint8_t publicKey[KEY_SIZE]);

void GenerateSessionKeys(
	const uint8_t privateKey[KEY_SIZE],
	const uint8_t publicKey[KEY_SIZE],
	const uint8_t peerPublicKey[KEY_SIZE],
	int64_t addition,
	uint8_t sessionKey1[KEY_SIZE],
	uint8_t sessionKey2[KEY_SIZE]);

void GenerateSignature(
	uint8_t seed[KEY_SIZE],
	uint8_t signaturePrivateKey[SIGNATURE_PRIVATE_KEY_SIZE],
	uint8_t signaturePublicKey[SIGNATURE_PUBLIC_KEY_SIZE]);

void GetSalt(String file, uint8_t salt[SALT_SIZE]);

#endif
