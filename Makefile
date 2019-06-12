
.PHONY:	build install clean
build:
	mkdir -p build install
	mkdir -p install/bin
	mkdir -p install/lib
	cd ./build && cmake ../ && make

install:
	cp build/src/libepserver.so install/lib
	cp build/example/echo_server install/bin
	cp -rf include install/include

clean:
	rm -rf build install
