#
# Makefile
#
# Helper Makefile for building and deploying the RoboHero Android Application.
#
# Copyright (C) 2026, Charles Chiou
#

JAVA_HOME ?= $(HOME)/.jdks/jdk-21.0.4+7
ANDROID_HOME ?= $(HOME)/Android/Sdk
export JAVA_HOME
export ANDROID_HOME
export PATH := $(JAVA_HOME)/bin:$(ANDROID_HOME)/platform-tools:$(PATH)

GRADLEW := ./gradlew

.PHONY: all default release debug bundle install clean

default: release
all: release

# Build production signed release APK (Default)
release:
	$(GRADLEW) assembleRelease

# Build development debug APK
debug:
	$(GRADLEW) assembleDebug

# Build Google Play Android App Bundle (AAB)
bundle:
	$(GRADLEW) bundleRelease

# Install built APK to connected device via ADB
install:
	@APK=$$(ls -t app/build/outputs/apk/release/RoboHero-*.apk 2>/dev/null | head -n 1); \
	if [ -z "$$APK" ]; then \
		APK=$$(ls -t app/build/outputs/apk/debug/RoboHero-*.apk 2>/dev/null | head -n 1); \
	fi; \
	if [ -z "$$APK" ]; then \
		echo "No built APK found. Run 'make release' or 'make debug' first." >&2; \
		exit 1; \
	fi; \
	echo "Installing $$APK..."; \
	adb install -r "$$APK"

# Clean build artifacts
clean:
	$(GRADLEW) clean
