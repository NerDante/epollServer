.PHONY:	build install clean
build:
	mkdir -p build install
	cd ./build && cmake ../ && make

install:
	cp build/src/libepserver.so install/

clean:
	rm -rf build install
