LINUX_CLANG_IMAGE ?= gaia-ecs-linux-clang

all:
	@echo "test - clean, build, and test"
	@echo "test-linux-clang - Debug unit tests with Ubuntu Clang in Docker"
	@echo "clean - remove built files"
	@echo "install - install"

install: clean
	mkdir build && cd build && cmake .. && cmake --build . --config Release --target install

test:
	rm -rf ./build || true
	cmake -S . -B build -DGAIA_BUILD_UNITTEST=ON
	cmake --build build
	ctest --test-dir build --output-on-failure

# Apple Clang does not compile POSIX semaphore code and does not emit Clang 18
# enumerator-shadow or unknown-warning-option diagnostics. This target matches
# GitHub coverage's Ubuntu Clang -Werror build.
test-linux-clang: linux-clang-image
	docker run --rm -v "$(CURDIR)":/src:ro -w /src \
		-e CC=clang -e CXX=clang++ \
		$(LINUX_CLANG_IMAGE) \
		bash -lc 'cmake -DCMAKE_BUILD_TYPE=Debug -DGAIA_BUILD_UNITTEST=ON -DGAIA_BUILD_EXAMPLES=OFF -DGAIA_BUILD_BENCHMARK=OFF -DGAIA_GENERATE_CC=OFF -S . -B /tmp/gaia-linux-clang && cmake --build /tmp/gaia-linux-clang --config Debug --target gaia_test -j$$(nproc)'

linux-clang-image:
	@docker image inspect $(LINUX_CLANG_IMAGE) >/dev/null 2>&1 || \
		printf '%s\n' \
			'FROM ubuntu:24.04' \
			'ENV DEBIAN_FRONTEND=noninteractive' \
			'RUN apt-get update && apt-get install -y --no-install-recommends clang cmake ninja-build make ca-certificates git && rm -rf /var/lib/apt/lists/*' \
		| docker build -t $(LINUX_CLANG_IMAGE) -

build:
	cmake -S . -B build -DGAIA_BUILD_UNITTEST=ON
	cmake --build build

clean:
	rm -rf ./build || true

.PHONY: all install test test-linux-clang linux-clang-image build clean