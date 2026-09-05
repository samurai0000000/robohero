#
# Makefile
#
# Copyright (C) 2026, Charles Chiou
#

APACHE_DIR ?= /var/www/html/robohero
NPM ?= npm

MODEL_DIR ?= ../../model
PUBLIC_MODEL_DIR := public/model
PUBLIC_MODEL_FILES := $(PUBLIC_MODEL_DIR)/robohero.urdf $(PUBLIC_MODEL_DIR)/calibration.json

.PHONY: all install sync-model build dev preview serve clean deploy

all: build

install:
	@$(NPM) install

$(PUBLIC_MODEL_DIR)/%: $(MODEL_DIR)/%
	@mkdir -p $(PUBLIC_MODEL_DIR)
	cp $< $@

sync-model: $(PUBLIC_MODEL_FILES)

build: install sync-model
	@$(NPM) run build

dev: install sync-model
	@$(NPM) run dev

preview: build
	npm run preview

serve: build
	npm run serve

clean:
	@rm -rf dist node_modules public/model

-include local.deploy.mk
