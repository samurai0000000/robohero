#
# Makefile
#
# Copyright (C) 2026, Charles Chiou
#

APACHE_DIR ?= /var/www/html/robohero
NPM ?= npm

.PHONY: all install build dev preview clean deploy

all: build

install:
	@$(NPM) install

build: install
	@$(NPM) run build

dev: install
	@$(NPM) run dev

preview: build
	npm run preview

serve: build
	npm run serve

clean:
	@rm -rf dist node_modules

-include local.deploy.mk
