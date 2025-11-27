export BUILD_DIR != echo `pwd`/build
INSTALL_DIR = /usr/local/bin

export C = gcc -Wall -O3
export CXX = g++ -Wall -O3
export OBJ_FLAG = -c
export STATIC_FLAG = -static

.PHONY: all server client clean install installserver installclient tests

all:
	cd src && $(MAKE) all

server:
	cd src && $(MAKE) server

client:
	cd src && $(MAKE) client

tests: all
	cd tests && $(MAKE)

clean:
	rm -rf $(BUILD_DIR)
	cd tests && $(MAKE) clean

install: installserver installclient

installserver: $(INSTALL_DIR)/talkd $(INSTALL_DIR)/talkdctl

installclient: $(INSTALL_DIR)/talk

$(BUILD_DIR)/talkd $(BUILD_DIR)/talkdctl: server

$(BUILD_DIR)/talk: client

$(INSTALL_DIR)/%: $(BUILD_DIR)/%
	cp $< $@
