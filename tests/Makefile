TEST_LIST = UserDB.Test Handshake.Test

.PHONY: all clean

all: $(TEST_LIST)

clean:
	rm -f $(TEST_LIST)

USERDB_MODULES =\
	Server/UserDB.o \
	Common/MyString.o \
	Common/BinaryFile.o \
	Common/UnixTime.o \
	ThirdParty/monocypher.o

USERDB_MODULES_ABS := $(USERDB_MODULES:%=$(BUILD_DIR)/%)

UserDB.Test: UserDB.Test.cpp $(USERDB_MODULES_ABS)
	$(CXX) $(STATIC_FLAG) -o $@ $< $(USERDB_MODULES_ABS)

HANDSHAKE_MODULES =\
	Server/UserDB.o \
	Server/MessagePipe.o \
	Protocol/Session.o \
	Protocol/ServerSession.o \
	Protocol/ActiveSession.o \
	Protocol/Handshake.o \
	Message/Message.o \
	Message/MessageStorage.o \
	Common/MyString.o \
	Common/BinaryFile.o \
	Common/File.o \
	Common/UnixTime.o \
	Crypto/Crypto.o \
	ThirdParty/monocypher.o

HANDSHAKE_MODULES_ABS := $(HANDSHAKE_MODULES:%=$(BUILD_DIR)/%)

Handshake.Test: Handshake.Test.cpp $(HANDSHAKE_MODULES_ABS)
	$(CXX) $(STATIC_FLAG) -o $@ $< $(HANDSHAKE_MODULES_ABS)

TREE_MODULES =\
	Common/MyString.o

TREE_MODULES_ABS := $(TREE_MODULES:%=$(BUILD_DIR)/%)

Tree.Test: Tree.Test.cpp $(TREE_MODULES_ABS)
	$(CXX) $(STATIC_FLAG) -o $@ $< $(TREE_MODULES_ABS)

FILETREE_MODULES =\
	Common/MyString.o \
	Common/BinaryFile.o

FILETREE_MODULES_ABS := $(FILETREE_MODULES:%=$(BUILD_DIR)/%)

FileTree.Test: FileTree.Test.cpp $(FILETREE_MODULES_ABS)
	$(CXX) $(STATIC_FLAG) -o $@ $< $(FILETREE_MODULES_ABS)
