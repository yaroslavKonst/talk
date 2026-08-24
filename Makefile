export BUILD_DIR != echo `pwd`/build
INSTALL_DIR = /usr/local/bin

export CC = gcc -Wall -Wshadow -O3 -g
export CXX = g++ -Wall -Wshadow -O3 -g
export OBJ_FLAG = -c
export STATIC_FLAG = -static

.PHONY: \
	all \
	server \
	client \
	utils \
	clean \
	install \
	installserver \
	installclient \
	tests \
	testrun

all: utils
	cd src && $(MAKE) all

server: utils
	cd src && $(MAKE) server

client: utils
	cd src && $(MAKE) client

utils:
	src/Utils/CodeCheck.sh
	cd src/Utils && $(MAKE)

tests: all
	cd tests && $(MAKE)

testrun: tests
	cd tests && $(MAKE) run

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
