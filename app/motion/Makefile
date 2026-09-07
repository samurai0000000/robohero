#
# Makefile
#
# Copyright (C) 2026, Charles Chiou
#

ifeq ($(OS),Windows_NT)
    BUILD_DIR ?= build/windows-msvc
    CONFIG ?= Release
    JOBS ?= $(or $(NUMBER_OF_PROCESSORS),4)
    CONFIGURE_CMD ?= cmake -B $(BUILD_DIR) --preset windows-msvc
    RUN_CMD ?= cmake -E chdir $(BUILD_DIR)/$(CONFIG) ./robohero_motion.exe
else
    BUILD_DIR ?= build/linux
    CONFIG ?= Release
    JOBS ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    CONFIGURE_CMD ?= cmake -B $(BUILD_DIR) -S .
    RUN_CMD ?= ./$(BUILD_DIR)/robohero_motion
endif

.PHONY: all build configure clean distclean run

all: build

build: $(BUILD_DIR)/CMakeCache.txt
	@cmake --build $(BUILD_DIR) --config $(CONFIG) -j$(JOBS)

$(BUILD_DIR)/CMakeCache.txt:
	@cmake -E make_directory $(BUILD_DIR)
	@$(CONFIGURE_CMD)

configure:
	@cmake -E make_directory $(BUILD_DIR)
	@$(CONFIGURE_CMD)

clean:
ifneq ($(wildcard $(BUILD_DIR)/CMakeCache.txt),)
	-@cmake --build $(BUILD_DIR) --config $(CONFIG) --target clean
endif

distclean:
	@cmake -E rm -rf build

run: build
	@$(RUN_CMD)
