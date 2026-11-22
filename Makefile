# Thin convenience wrapper around CMake. The real build lives in CMakeLists.txt.
#
#   make                 configure + build into $(BUILD_DIR)
#   make test            run the unit tests
#   make run             run the pusher demo
#   make install PREFIX=/opt/tp

BUILD_TYPE ?= Release
BUILD_DIR  ?= build
PREFIX     ?= /usr/local
GENERATOR  ?=
CMAKE      ?= cmake
CTEST      ?= ctest
JOBS       ?= 4
CMAKE_ARGS ?=

# Pusher demo arguments: [config_file] [num_pushers] [seconds]
RUN_ARGS   ?= threadpool.ini 2 5

ifneq ($(strip $(GENERATOR)),)
GENERATOR_FLAG := -G "$(GENERATOR)"
endif

.PHONY: all configure build test run bench install clean rebuild help

all: build

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) $(GENERATOR_FLAG) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(CMAKE_ARGS)

build: configure
	$(CMAKE) --build $(BUILD_DIR) --config $(BUILD_TYPE) --parallel $(JOBS)

test: build
	$(CTEST) --test-dir $(BUILD_DIR) -C $(BUILD_TYPE) --output-on-failure

# Multi-config generators (Visual Studio, Ninja Multi-Config) put binaries in
# $(BUILD_DIR)/$(BUILD_TYPE). Recursive '=' resolves this after the build ran.
BIN_DIR = $(if $(wildcard $(BUILD_DIR)/$(BUILD_TYPE)/.),$(BUILD_DIR)/$(BUILD_TYPE),$(BUILD_DIR))

run: build
	cd $(BIN_DIR) && ./tp_pusher_demo $(RUN_ARGS)

bench: build
	cd $(BIN_DIR) && ./tp_bench_throughput

install: build
	$(CMAKE) --install $(BUILD_DIR) --config $(BUILD_TYPE) --prefix "$(PREFIX)"

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean all

help:
	@echo "Usage: make [target] [BUILD_TYPE=Release|Debug] [BUILD_DIR=build]"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Configure and build (default)"
	@echo "  test     - Build and run the unit tests (ctest)"
	@echo "  run      - Build and run the pusher demo (RUN_ARGS='$(RUN_ARGS)')"
	@echo "  bench    - Build and run the throughput benchmark"
	@echo "  install  - Install headers, library and CMake package to PREFIX ($(PREFIX))"
	@echo "  clean    - Remove $(BUILD_DIR)/"
	@echo "  rebuild  - clean + all"
	@echo "  help     - Show this message"
	@echo ""
	@echo "Variables: GENERATOR (e.g. Ninja), CMAKE_ARGS (extra -D flags), JOBS"
