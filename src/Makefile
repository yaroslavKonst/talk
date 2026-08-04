SERVER_MODULES =\
	Server/Server.o \
	Server/ServerMain.o \
	Server/User.o \
	Server/UserDB.o \
	Server/ListeningSocket.o \
	Server/ControlSocket.o \
	Server/ControlSession.o \
	Server/Config.o \
	Server/ServerHandshake.o \
	Server/ServerSession.o \
	Server/GateListeningSocket.o \
	Server/InboundGateSession.o \
	Server/OutboundGateSession.o \
	Server/SendPlanner.o \
	Server/ObjectType.o \
	Server/FailBan.o \
	Server/RateLimiter.o \
	Server/ShutdownSignalController.o \
	Message/Message.o \
	Message/ContactStorage.o \
	Protocol/ParserHelpers.o \
	Protocol/HandshakeParser.o \
	Protocol/SessionProtocol.o \
	Protocol/SessionParser.o \
	Protocol/ControlParser.o \
	Protocol/GateParser.o \
	Common/EventDispatcher.o \
	Common/MyString.o \
	Common/UnixTime.o \
	Common/File.o \
	Common/IniFile.o \
	Common/BinaryFile.o \
	Common/SignalHandling.o \
	Common/CommandLineParser.o \
	Common/Resolver.o \
	Common/Version.o \
	Common/StreamReader.o \
	Common/StreamWriter.o \
	Common/ObjectStorage.o \
	Common/Log.o \
	Crypto/Crypto.o \
	ThirdParty/monocypher.o

SERVERCTL_MODULES=\
	ServerCtl/ServerCtlMain.o \
	ServerCtl/RequestBuilder.o \
	ServerCtl/ResponseProcessor.o \
	Protocol/ParserHelpers.o \
	Protocol/ControlParser.o \
	Common/UnixTime.o \
	Common/MyString.o \
	Common/Version.o \
	Common/StreamReader.o \
	Common/StreamWriter.o \
	Common/CommandLineParser.o \
	ThirdParty/monocypher.o

CLIENT_MODULES=\
	Client/Client.o \
	Client/ClientMain.o \
	Client/Root.o \
	Client/Config.o \
	Client/Network.o \
	Client/ChatList.o \
	Client/Chat.o \
	Client/TextEditor.o \
	Client/ClientHandshake.o \
	Client/ClientSession.o \
	Client/UI.o \
	Client/UiHelpers.o \
	Client/Screen.o \
	Client/WorkScreen.o \
	Client/LoginScreen.o \
	Client/ContactScreen.o \
	Client/ContactManageScreen.o \
	Client/ContactListScreen.o \
	Client/MessageDescriptor.o \
	Client/MessageDraft.o \
	Client/NotificationSystem.o \
	Message/ContactStorage.o \
	Message/Message.o \
	Protocol/ParserHelpers.o \
	Protocol/HandshakeParser.o \
	Protocol/SessionProtocol.o \
	Protocol/SessionParser.o \
	Common/MyString.o \
	Common/UTF8.o \
	Common/Version.o \
	Common/Resolver.o \
	Common/SignalHandling.o \
	Common/CommandLineParser.o \
	Common/IniFile.o \
	Common/BinaryFile.o \
	Common/File.o \
	Common/EventDispatcher.o \
	Common/UnixTime.o \
	Common/StreamReader.o \
	Common/StreamWriter.o \
	Common/ObjectStorage.o \
	Common/Log.o \
	Crypto/Crypto.o \
	ThirdParty/monocypher.o

CLIENT_LIBS = -lncursesw -lpulse-simple -pthread

.PHONY: all server client

all: server client

server: $(BUILD_DIR)/talkd $(BUILD_DIR)/talkdctl

client: $(BUILD_DIR)/talk

SERVER_MODULES_ABS := $(SERVER_MODULES:%=$(BUILD_DIR)/%)
SERVERCTL_MODULES_ABS := $(SERVERCTL_MODULES:%=$(BUILD_DIR)/%)
CLIENT_MODULES_ABS := $(CLIENT_MODULES:%=$(BUILD_DIR)/%)

BUILD_DIRS := $(shell find . -type d)
BUILD_DIRS := $(BUILD_DIRS:%=$(BUILD_DIR)/%)

# Server
$(BUILD_DIR)/talkd: $(SERVER_MODULES_ABS) | $(BUILD_DIRS)
	$(CXX) $(STATIC_FLAG) -o $@ $(SERVER_MODULES_ABS)

# Server control
$(BUILD_DIR)/talkdctl: $(SERVERCTL_MODULES_ABS) | $(BUILD_DIRS)
	$(CXX) $(STATIC_FLAG) -o $@ $(SERVERCTL_MODULES_ABS)

# Client
$(BUILD_DIR)/talk: $(CLIENT_MODULES_ABS) | $(BUILD_DIRS)
	$(CXX) -o $@ $(CLIENT_MODULES_ABS) $(CLIENT_LIBS)

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIRS)
	$(CXX) $(ENDIANNESS) $(OBJ_FLAG) -o $@ $<

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIRS)
	$(CC) $(ENDIANNESS) $(OBJ_FLAG) -o $@ $<

$(BUILD_DIRS):
	mkdir -p $(BUILD_DIRS)

DEPS_HEADER := $(shell find . -name *.hpp)
DEPS_SOURCE := $(shell find . -name *.cpp)
DEPS_SOURCE := $(subst ./,,$(DEPS_SOURCE))
DEPS_RES := $(DEPS_SOURCE:%.cpp=-MT:$(BUILD_DIR)/%.o:)
DEPS_ARGS := $(join $(DEPS_RES),$(DEPS_SOURCE))

$(BUILD_DIR)/deps.mk: $(DEPS_SOURCE) $(DEPS_HEADER) | $(BUILD_DIRS)
	truncate -s 0 $@
	for ENTRY in $(DEPS_ARGS) ; do $(CXX) $(ENDIANNESS) -MM `echo $$ENTRY | sed "s|:| |g"` >> $@ ; done

include $(BUILD_DIR)/deps.mk

ENDIANNESS := -D$(shell $(BUILD_DIR)/Utils/EndiannessChecker)
