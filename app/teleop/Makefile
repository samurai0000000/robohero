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
    RUN_CMD ?= cmake -E chdir $(BUILD_DIR)/$(CONFIG) ./robohero_teleop.exe

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

else
    LINUX_BUILD_DIR := build/linux
    WINDOWS_BUILD_DIR := build/windows
    VERSION := $(shell sed -n 's/^.*<!--[[:space:]]*robohero-version:[[:space:]]*\([0-9]\+\.[0-9]\+\.[0-9]\+\)[[:space:]]*-->.*$$/\1/p' ../../README.md)
    WINDOWS_INSTALLER := $(WINDOWS_BUILD_DIR)/robohero_teleop-setup-$(VERSION).exe
    CMAKE := $(shell if [ -x /usr/bin/cmake ]; then echo /usr/bin/cmake; else echo cmake; fi)
    CTEST := $(shell if [ -x /usr/bin/ctest ]; then echo /usr/bin/ctest; else echo ctest; fi)
    JOBS ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    MINGW_CC := x86_64-w64-mingw32-gcc-posix
    MINGW_CXX := x86_64-w64-mingw32-g++-posix
    MINGW_RC := x86_64-w64-mingw32-windres

    SANITIZE ?= 0
    ifneq (,$(findstring -fsanitize,$(CFLAGS) $(CXXFLAGS)))
        SANITIZE := 1
    endif

    ifeq ($(SANITIZE),1)
        TEST_BUILD_DIR := build/linux-sanitize
        TEST_CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DENABLE_SANITIZERS=ON
    else
        TEST_BUILD_DIR := $(LINUX_BUILD_DIR)
        TEST_CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DENABLE_SANITIZERS=OFF
    endif

.PHONY: all build linux windows installer test clean distclean run

all: linux windows

build: all

linux:
	@if [ ! -f "$(LINUX_BUILD_DIR)/CMakeCache.txt" ]; then \
		$(CMAKE) -S . -B $(LINUX_BUILD_DIR) -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DENABLE_SANITIZERS=OFF; \
	fi
	@$(CMAKE) --build $(LINUX_BUILD_DIR) -j$(JOBS)

windows:
	@if [ ! -f "$(WINDOWS_BUILD_DIR)/CMakeCache.txt" ]; then \
		$(CMAKE) -S . -B $(WINDOWS_BUILD_DIR) \
			-DCMAKE_BUILD_TYPE=Release \
			-DCMAKE_SYSTEM_NAME=Windows \
			-DCMAKE_C_COMPILER=$(MINGW_CC) \
			-DCMAKE_CXX_COMPILER=$(MINGW_CXX) \
			-DCMAKE_RC_COMPILER=$(MINGW_RC) \
			-DBUILD_TESTING=OFF; \
	fi
	@$(CMAKE) --build $(WINDOWS_BUILD_DIR) -j$(JOBS)
	@test -f "$(WINDOWS_INSTALLER)"

installer: windows

test:
	@if [ ! -f "$(TEST_BUILD_DIR)/CMakeCache.txt" ]; then \
		$(CMAKE) -S . -B $(TEST_BUILD_DIR) $(TEST_CMAKE_FLAGS); \
	fi
	@$(CMAKE) --build $(TEST_BUILD_DIR) -j$(JOBS)
	@QT_QPA_PLATFORM=offscreen ROBOHERO_DISABLE_AUTO_MQTT=1 ROBOHERO_DISABLE_AUTO_CAMERA=1 $(CTEST) --test-dir $(TEST_BUILD_DIR) --output-on-failure

clean:
	@rm -rf build/linux build/linux-sanitize $(WINDOWS_BUILD_DIR)

distclean: clean
	@rm -rf build

run: linux
	@./$(LINUX_BUILD_DIR)/robohero_teleop
endif
