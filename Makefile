# Makefile
#
# Copyright (C) 2024, Charles Chiou

TARGETS =	build/esp8266/robohero_arduino.ino.bin

.PHONY: default clean distclean

default: $(TARGETS)

clean:
	rm -rf build

distclean:
	rm -rf build

.PHONY: robohero

build/esp8266/robohero_arduino.ino.bin: $(wildcard robohero_arduino/*.*)
	@mkdir -p $(dir $@)
	@arduino-builder -compile \
		-logger=human \
		-hardware /usr/share/arduino/hardware \
		-hardware $(HOME)/.arduino15/packages \
		-tools /usr/share/arduino/hardware/tools/avr \
		-tools $(HOME)/.arduino15/packages \
		-libraries $(HOME)/Arduino/libraries \
		-fqbn=esp8266:esp8266:generic:CpuFrequency=80,FlashFreq=40,FlashMode=dio,UploadSpeed=115200,FlashSize=2M,ResetMethod=nodemcu,Debug=Disabled,DebugLevel=None____ \
		-ide-version=10819 \
		-build-path $(realpath $(dir $@)) \
		-build-cache $(realpath $(dir $@)) \
		-warnings=none \
		-prefs=build.warn_data_percentage=75 \
		-prefs=runtime.tools.esptool.path=$(HOME)/.arduino15/packages/esp8266/tools/esptool/0.4.9 \
		-prefs=runtime.tools.esptool-0.4.9.path=$(HOME)/.arduino15/packages/esp8266/tools/esptool/0.4.9 \
		-prefs=runtime.tools.mkspiffs.path=$(HOME)/.arduino15/packages/esp8266/tools/mkspiffs/0.1.2 \
		-prefs=runtime.tools.mkspiffs-0.1.2.path=$(HOME)/.arduino15/packages/esp8266/tools/mkspiffs/0.1.2 \
		-prefs=runtime.tools.xtensa-lx106-elf-gcc.path=$(HOME)/.arduino15/packages/esp8266/tools/xtensa-lx106-elf-gcc/1.20.0-26-gb404fb9-2 \
		-prefs=runtime.tools.xtensa-lx106-elf-gcc-1.20.0-26-gb404fb9-2.path=$(HOME)/.arduino15/packages/esp8266/tools/xtensa-lx106-elf-gcc/1.20.0-26-gb404fb9-2 \
		-verbose \
		$<
