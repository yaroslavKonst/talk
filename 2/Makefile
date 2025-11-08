export BUILD_DIR != echo `pwd`/build

export C = gcc -Wall -O3
export CXX = g++ -Wall -O3
export OBJ_FLAG = -c

.PHONY: all clean

all:
	cd src && $(MAKE)

clean:
	rm -rf $(BUILD_DIR)
