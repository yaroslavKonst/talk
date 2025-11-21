export BUILD_DIR != echo `pwd`/build

export C = gcc -Wall -O3
export CXX = g++ -Wall -O3
export OBJ_FLAG = -c
export STATIC_FLAG = -static

.PHONY: all clean install

all:
	cd src && $(MAKE)

clean:
	rm -rf $(BUILD_DIR)

install: all
	cd build && cp talkd talkdctl talk /usr/local/bin
