SERVER_MODULES =\
	Server/Server.o \
	Server/ServerMain.o \
	Server/UserDB.o \
	Protocol/Protocol.o \
	Common/MyString.o \
	Common/IniFile.o \
	Common/BinaryFile.o \
	Crypto/Crypto.o \
	ThirdParty/monocypher.o

SERVER_MODULES_ABS := $(SERVER_MODULES:%=$(BUILD_DIR)/%)

BUILD_DIRS := $(shell find . -type d)
BUILD_DIRS := $(BUILD_DIRS:%=$(BUILD_DIR)/%)

# Server
$(BUILD_DIR)/talkd: $(SERVER_MODULES_ABS) | $(BUILD_DIRS)
	$(CXX) -o $@ $(SERVER_MODULES_ABS)

# Server control
# Client

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIRS)
	$(CXX) $(OBJ_FLAG) -o $@ $<

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIRS)
	$(C) $(OBJ_FLAG) -o $@ $<

$(BUILD_DIRS):
	mkdir -p $(BUILD_DIRS)

