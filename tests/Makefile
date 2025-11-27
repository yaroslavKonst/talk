TEST_LIST = UserDB.Test

.PHONY: all clean

all: $(TEST_LIST)

clean:
	rm $(TEST_LIST)

USERDB_MODULES =\
	Server/UserDB.o \
	Common/MyString.o \
	Common/BinaryFile.o \
	Common/UnixTime.o \
	ThirdParty/monocypher.o

USERDB_MODULES_ABS := $(USERDB_MODULES:%=$(BUILD_DIR)/%)

UserDB.Test: UserDB.Test.cpp $(USERDB_MODULES_ABS)
	$(CXX) $(STATIC_FLAG) -o $@ $< $(USERDB_MODULES_ABS)
