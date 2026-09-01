#
# Makefile
#
# Copyright (C) 2026, Charles Chiou
#

BUILD_DIR ?= build

.PHONY: all clean distclean run

all:
	@mkdir -p $(BUILD_DIR)
	@cmake -B $(BUILD_DIR) -S .
	@cmake --build $(BUILD_DIR)

clean:
	@if [ -d "$(BUILD_DIR)" ]; then \
		cmake --build $(BUILD_DIR) --target clean 2>/dev/null || true; \
	fi

distclean:
	@rm -rf $(BUILD_DIR)

run: all
	@./$(BUILD_DIR)/robohero-host
