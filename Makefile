
.PHONY:	build install clean
build:
	mkdir -p build install
	mkdir -p install/bin
	mkdir -p install/lib
	cd ./build && cmake ../ && make

install:
	cp build/src/libepserver.so install/lib

clean:
	rm -rf build install
