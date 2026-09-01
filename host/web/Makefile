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
	@$(NPM) run preview

clean:
	@rm -rf dist node_modules

deploy: build
	@echo "Deploying static assets to $(APACHE_DIR)..."
	@sudo mkdir -p $(APACHE_DIR)
	@sudo cp -r dist/* $(APACHE_DIR)/
	@echo "Deployment complete."
