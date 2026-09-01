#
# Makefile
#
# Copyright (C) 2026, Charles Chiou
#

SUBDIRS := cli web

.PHONY: all clean distclean $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	@$(MAKE) -C $@ all

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

distclean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir distclean; \
	done
