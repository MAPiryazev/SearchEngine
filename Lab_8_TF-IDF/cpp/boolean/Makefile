BUILD_DIR := build
PREFIX := /usr/local

.PHONY: all configure build clean rebuild \
	run_tokenizer run_index_builder run_bool_search \
	run_index_builder6 run_bool_search6 run_rank_search8

all: build

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_PREFIX_PATH=$(PREFIX)

build: configure
	cmake --build $(BUILD_DIR)

run_tokenizer: build
	./$(BUILD_DIR)/tokenizer

run_index_builder: build
	./$(BUILD_DIR)/index_builder --out out

run_bool_search: build
	./$(BUILD_DIR)/bool_search --inv out/index.inv --fwd out/index.fwd --q "python OR java" --top 5

run_index_builder6: build
	./$(BUILD_DIR)/index_builder6 --out out6

run_bool_search6: build
	./$(BUILD_DIR)/bool_search6 --inv out6/index6.inv --fwd out6/index6.fwd --q "python OR java" --top 5

run_rank_search8: build
	./$(BUILD_DIR)/rank_search8 --inv out6/index6.inv --fwd out6/index6.fwd --q "python OR java" --top 5

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean build
