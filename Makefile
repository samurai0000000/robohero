#
# Makefile
#
# Copyright (C) 2026, Charles Chiou
#
# ESP8266_RTOS_SDK project. Build, flash, and monitor only.
#

ifeq ($(IDF_PATH),)
export IDF_PATH := $(CURDIR)/ESP8266_RTOS_SDK
endif

XTENSA_PATH ?= $(HOME)/esp/xtensa-lx106-elf/bin
ifneq ($(wildcard $(XTENSA_PATH)/xtensa-lx106-elf-gcc),)
export PATH := $(XTENSA_PATH):$(PATH)
endif
ifneq ($(wildcard $(HOME)/esp/bin/gperf),)
export PATH := $(HOME)/esp/bin:$(PATH)
endif

PROJECT_NAME := robohero

# libstdc++ thread-safe function-local statics call pthread_cond_wait
# and throw on failure. ESP8266 pthread cannot support that; the first
# Meyers singleton then aborts in libgcc unwind.
EXTRA_CXXFLAGS += -fno-threadsafe-statics


# Standalone wipe so distclean works even if the SDK tree is incomplete.
# Does not touch sdkconfig.defaults.
ifeq ($(filter distclean,$(MAKECMDGOALS)),distclean)
ifneq ($(filter-out distclean,$(MAKECMDGOALS)),)
$(error distclean cannot be combined with other targets)
endif
.PHONY: distclean
distclean:
	rm -rf build sdkconfig sdkconfig.old
else
# SDK runs menuconfig when sdkconfig is missing. Seed from defaults so
# a plain "make" stays non-interactive (Python confgen fills the rest).
ifeq ($(wildcard $(CURDIR)/sdkconfig),)
$(info Seeding sdkconfig from sdkconfig.defaults)
$(shell cp "$(CURDIR)/sdkconfig.defaults" "$(CURDIR)/sdkconfig")
endif

# IDF app descriptor comes from project(... VERSION ...) in CMakeLists.txt.
PROJECT_VER := $(shell python3 "$(CURDIR)/scripts/gen_version.py")
ifeq ($(PROJECT_VER),)
$(error Failed to generate version.h)
endif
$(info Generating version.h $(PROJECT_VER))

include $(IDF_PATH)/make/project.mk
endif
