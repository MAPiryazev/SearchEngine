BUILD_DIR ?= build
CMAKE ?= cmake
CTEST ?= ctest
CONFIG ?= Release

.PHONY: all configure build test run clean

all: test

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(CONFIG)

build: configure
	$(CMAKE) --build $(BUILD_DIR) -j

test: build
	$(CTEST) --test-dir $(BUILD_DIR) --output-on-failure

run: build
	./$(BUILD_DIR)/unit_tests -s

clean:
	rm -rf $(BUILD_DIR)
