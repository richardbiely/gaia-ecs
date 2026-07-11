all:
	@echo "test - clean, build, and test"
	@echo "clean - remove built files"
	@echo "install - install"

install: clean
	mkdir build && cd build && cmake .. && cmake --build . --config Release --target install

test:
	rm -rf ./build || true
	cmake -S . -B build -DGAIA_BUILD_UNITTEST=ON
	cmake --build build
	ctest --test-dir build --output-on-failure

build:
	cmake -S . -B build -DGAIA_BUILD_UNITTEST=ON
	cmake --build build

clean:
	rm -rf ./build || true

.PHONY: all install test build clean