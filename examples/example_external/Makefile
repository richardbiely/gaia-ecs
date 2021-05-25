all:
	@echo "run - clean, build, and run"
	@echo "clean - remove built files"

run: clean build

build:
	cd ../.. && $(MAKE) install
	mkdir build && cd build && cmake .. && $(MAKE) run

clean:
	rm -rf ./build || true