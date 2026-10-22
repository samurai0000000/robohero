#
# Makefile
#
# Copyright (C) 2026, Charles Chiou
#

export PATH := /usr/bin:$(PATH)

.PHONY: all linux windows installer cli teleop motion android test clean distclean

all: linux windows android

linux:
	@$(MAKE) -C app/cli all
	@$(MAKE) -C app/teleop linux
	@$(MAKE) -C app/motion linux

windows:
	@$(MAKE) -C app/teleop windows
	@$(MAKE) -C app/motion windows

installer: windows

cli:
	@$(MAKE) -C app/cli all

teleop:
	@$(MAKE) -C app/teleop all

motion:
	@$(MAKE) -C app/motion all

android:
	@$(MAKE) -C app/android release

test:
	@if grep -q '^test:' app/cli/Makefile 2>/dev/null; then \
		$(MAKE) -C app/cli test; \
	else \
		$(MAKE) -C app/cli all; \
	fi
	@$(MAKE) -C app/motion test $(if $(SANITIZE),SANITIZE=$(SANITIZE))
	@$(MAKE) -C app/teleop test $(if $(SANITIZE),SANITIZE=$(SANITIZE))

clean:
	@$(MAKE) -C app/cli clean
	@$(MAKE) -C app/teleop clean
	@$(MAKE) -C app/motion clean

distclean:
	@$(MAKE) -C app/cli distclean
	@$(MAKE) -C app/teleop distclean
	@$(MAKE) -C app/motion distclean
