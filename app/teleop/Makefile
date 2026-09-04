#
# Makefile
#
# Copyright (C) 2026, Charles Chiou
#

BUILD_DIR ?= build/linux
JOBS ?= $(shell nproc 2>/dev/null || echo 4)

.PHONY: all build configure clean distclean run

all: build

build:
	@mkdir -p $(BUILD_DIR)
	@if [ ! -f "$(BUILD_DIR)/CMakeCache.txt" ]; then \
		cmake -B $(BUILD_DIR) -S . ; \
	fi
	@cmake --build $(BUILD_DIR) -j$(JOBS)

configure:
	@cmake -B $(BUILD_DIR) -S .

clean:
	@if [ -d "$(BUILD_DIR)" ]; then \
		cmake --build $(BUILD_DIR) --target clean 2>/dev/null || true; \
	fi

distclean:
	@rm -rf build

run: build
	@./$(BUILD_DIR)/robohero_teleop
