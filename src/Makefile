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

SERVERCTL_MODULES=\
	ServerCtl/ServerCtlMain.o \
	Common/MyString.o

CLIENT_MODULES=\
	Client/Client.o \
	Client/ClientMain.o \
	Client/UI.o \
	Server/UserDB.o \
	Protocol/Protocol.o \
	Common/MyString.o \
	Common/BinaryFile.o \
	Crypto/Crypto.o \
	ThirdParty/monocypher.o

CLIENT_LIBS = -lncurses

.PHONY: all

all: $(BUILD_DIR)/talkd $(BUILD_DIR)/talkdctl $(BUILD_DIR)/talk

SERVER_MODULES_ABS := $(SERVER_MODULES:%=$(BUILD_DIR)/%)
SERVERCTL_MODULES_ABS := $(SERVERCTL_MODULES:%=$(BUILD_DIR)/%)
CLIENT_MODULES_ABS := $(CLIENT_MODULES:%=$(BUILD_DIR)/%)

BUILD_DIRS := $(shell find . -type d)
BUILD_DIRS := $(BUILD_DIRS:%=$(BUILD_DIR)/%)

# Server
$(BUILD_DIR)/talkd: $(SERVER_MODULES_ABS) | $(BUILD_DIRS)
	$(CXX) -o $@ $(SERVER_MODULES_ABS)

# Server control
$(BUILD_DIR)/talkdctl: $(SERVERCTL_MODULES_ABS) | $(BUILD_DIRS)
	$(CXX) -o $@ $(SERVERCTL_MODULES_ABS)

# Client
$(BUILD_DIR)/talk: $(CLIENT_MODULES_ABS) | $(BUILD_DIRS)
	$(CXX) -o $@ $(CLIENT_MODULES_ABS) $(CLIENT_LIBS)

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIRS)
	$(CXX) $(OBJ_FLAG) -o $@ $<

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIRS)
	$(C) $(OBJ_FLAG) -o $@ $<

$(BUILD_DIRS):
	mkdir -p $(BUILD_DIRS)
