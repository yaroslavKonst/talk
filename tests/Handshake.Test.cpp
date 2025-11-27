#include <sys/socket.h>
#include <cstring>
#include <cstdio>

#include "../src/Protocol/ServerSession.hpp"
#include "../src/Common/UnixTime.hpp"

void TestInvalidSize(UserDB *users)
{
	printf("Test invalid size.\n");

	int input[2];
	socketpair(AF_UNIX, SOCK_STREAM, 0, input);

	ServerSession session;
	session.Socket = input[1];

	session.Users = users;
	session.Pipe = nullptr;

	session.State = ServerSession::ServerStateWaitFirstSyn;
	session.SignatureKey = nullptr;
	session.PeerPublicKey = nullptr;
	session.PublicKey = nullptr;
	session.PrivateKey = nullptr;
	session.VoiceState = ServerSession::VoiceStateInactive;
	session.VoicePeer = nullptr;

	uint64_t size = 1025;
	int wb = write(input[0], &size, sizeof(size));

	if (wb != sizeof(size)) {
		printf("Failed to write to socket.\n");
	}

	bool res = session.Read();

	if (res) {
		printf("Failure.\n");
	} else {
		printf("Success.\n");
	}

	session.Socket = -1;

	close(input[0]);
	close(input[1]);
}

void TestInvalidKey(UserDB *users)
{
	printf("Test invalid key.\n");

	int input[2];
	socketpair(AF_UNIX, SOCK_STREAM, 0, input);

	ServerSession session;
	session.Socket = input[1];

	session.Users = users;
	session.Pipe = nullptr;

	session.State = ServerSession::ServerStateWaitFirstSyn;
	session.SignatureKey = nullptr;
	session.PeerPublicKey = nullptr;
	session.PublicKey = nullptr;
	session.PrivateKey = nullptr;
	session.VoiceState = ServerSession::VoiceStateInactive;
	session.VoicePeer = nullptr;

	CowBuffer<uint8_t> inputBuffer(KEY_SIZE + sizeof(int64_t) +
		SIGNATURE_SIZE);

	memset(inputBuffer.Pointer(), 2, inputBuffer.Size());
	inputBuffer = ApplyScrambler(inputBuffer);

	CowBuffer<uint8_t> sizeBuffer(sizeof(uint64_t));
	*sizeBuffer.SwitchType<uint64_t>() = inputBuffer.Size();

	inputBuffer = sizeBuffer.Concat(inputBuffer);

	uint64_t wb =
		write(input[0], inputBuffer.Pointer(), inputBuffer.Size());

	if (wb != inputBuffer.Size()) {
		printf("Failed to write to socket.\n");
	}

	while (!session.CanReceive()) {
		bool res = session.Read();

		if (!res) {
			printf("Failed to receive data.\n");
			break;
		}
	}

	bool res = session.Process();

	if (res) {
		printf("Failure.\n");
	} else {
		printf("Success.\n");
	}

	session.Socket = -1;

	close(input[0]);
	close(input[1]);
}

void TestInvalidSignature(UserDB *users)
{
	printf("Test invalid signature.\n");
	MessagePipe mPipe;

	int input[2];
	socketpair(AF_UNIX, SOCK_STREAM, 0, input);

	ServerSession session;
	session.Socket = input[1];

	session.Users = users;
	session.Pipe = &mPipe;

	session.State = ServerSession::ServerStateWaitFirstSyn;
	session.SignatureKey = nullptr;
	session.PeerPublicKey = nullptr;
	session.PublicKey = nullptr;
	session.PrivateKey = nullptr;
	session.VoiceState = ServerSession::VoiceStateInactive;
	session.VoicePeer = nullptr;

	CowBuffer<uint8_t> inputBuffer(KEY_SIZE + sizeof(int64_t) +
		SIGNATURE_SIZE);

	memset(inputBuffer.Pointer(), 1, inputBuffer.Size());
	inputBuffer = ApplyScrambler(inputBuffer);

	CowBuffer<uint8_t> sizeBuffer(sizeof(uint64_t));
	*sizeBuffer.SwitchType<uint64_t>() = inputBuffer.Size();

	inputBuffer = sizeBuffer.Concat(inputBuffer);

	uint64_t wb =
		write(input[0], inputBuffer.Pointer(), inputBuffer.Size());

	if (wb != inputBuffer.Size()) {
		printf("Failed to write to socket.\n");
	}

	while (!session.CanReceive()) {
		bool res = session.Read();

		if (!res) {
			printf("Failed to receive data.\n");
			break;
		}
	}

	bool res = session.Process();

	if (res) {
		printf("Failure.\n");
	} else {
		printf("Success.\n");
	}

	session.Socket = -1;

	close(input[0]);
	close(input[1]);
}

int main(int argc, char **argv)
{
	UserDB users;
	uint8_t key[KEY_SIZE];
	memset(key, 1, KEY_SIZE);
	users.AddUser(key, key, GetUnixTime(), "test user");

	TestInvalidSize(&users);
	TestInvalidKey(&users);
	TestInvalidSignature(&users);

	unlink("talkd.users");
	return 0;
}
