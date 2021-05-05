all:
	@echo "test - clean, build, and test"
	@echo "clean - remove built files"
	@echo "install - install"

install: clean
	mkdir build && cd build && cmake .. && cmake --build . --config Release --target install

test: clean build

build:
	mkdir build && cd build && cmake .. && $(MAKE) test

clean:
	rm -rf ./build || true