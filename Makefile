# Makefile
#
# Copyright (C) 2026, Charles Chiou
#

ARDUINO15_ROOT ?= $(HOME)/.arduino15
ESP8266_TOOLS  ?= $(ARDUINO15_ROOT)/packages/esp8266/tools
ESP8266_CORE   ?= $(ARDUINO15_ROOT)/packages/esp8266/hardware/esp8266/2.3.0
ESPTOOL        ?= $(ESP8266_TOOLS)/esptool/0.4.9/esptool
EBOOT          ?= $(ESP8266_CORE)/bootloaders/eboot/eboot.elf

-include local.config.mk

export AP_PASSWORD
export CLIENT_SSID
export CLIENT_PASSWORD

CMAKE_DEFS =
ifdef AP_PASSWORD
CMAKE_DEFS += -DAP_PASSWORD="$(AP_PASSWORD)"
endif
ifdef CLIENT_SSID
CMAKE_DEFS += -DCLIENT_SSID="$(CLIENT_SSID)"
endif
ifdef CLIENT_PASSWORD
CMAKE_DEFS += -DCLIENT_PASSWORD="$(CLIENT_PASSWORD)"
endif

TARGETS =	build/robohero.bin

.PHONY: default all clean distclean robohero legacy

default: $(TARGETS)

all: $(TARGETS) build/esp8266/robohero_arduino.ino.bin

robohero: $(TARGETS)

clean:
	@test -f build/Makefile && $(MAKE) -C build clean || true
	rm -f build/robohero.bin

distclean:
	rm -rf build

build/Makefile: CMakeLists.txt cmake/override.cmake $(wildcard local.config.mk)
	@mkdir -p build && cd build && cmake .. $(CMAKE_DEFS)

build/robohero: build/Makefile $(wildcard *.hxx) $(wildcard *.cxx) $(wildcard cmake/*)
	@$(MAKE) -C build robohero

build/robohero.bin: build/robohero
	@echo "Converting ELF to ESP8266 binary: $@"
	@$(ESPTOOL) -eo $(EBOOT) \
		-bo $@ \
		-bm dio -bf 40 -bz 2M -bs .text -bp 4096 -ec \
		-eo $< -bs .irom0.text -bs .text -bs .data -bs .rodata -bc -ec

# Legacy Arduino ino target
legacy: build/esp8266/robohero_arduino.ino.bin

build/esp8266/robohero_arduino.ino.bin: $(wildcard robohero_arduino/*.*)
	@mkdir -p $(dir $@)
	@arduino-builder -compile \
		-logger=human \
		-hardware /usr/share/arduino/hardware \
		-hardware $(ARDUINO15_ROOT)/packages \
		-tools /usr/share/arduino/hardware/tools/avr \
		-tools $(ARDUINO15_ROOT)/packages \
		-libraries $(HOME)/Arduino/libraries \
		-fqbn=esp8266:esp8266:generic:CpuFrequency=80,FlashFreq=40,FlashMode=dio,UploadSpeed=115200,FlashSize=2M,ResetMethod=nodemcu,Debug=Disabled,DebugLevel=None____ \
		-ide-version=10819 \
		-build-path $(abspath $(dir $@)) \
		-build-cache $(abspath $(dir $@)) \
		-warnings=none \
		-prefs=build.warn_data_percentage=75 \
		-prefs=runtime.tools.esptool.path=$(ESP8266_TOOLS)/esptool/0.4.9 \
		-prefs=runtime.tools.esptool-0.4.9.path=$(ESP8266_TOOLS)/esptool/0.4.9 \
		-prefs=runtime.tools.mkspiffs.path=$(ESP8266_TOOLS)/mkspiffs/0.1.2 \
		-prefs=runtime.tools.mkspiffs-0.1.2.path=$(ESP8266_TOOLS)/mkspiffs/0.1.2 \
		-prefs=runtime.tools.xtensa-lx106-elf-gcc.path=$(ESP8266_TOOLS)/xtensa-lx106-elf-gcc/1.20.0-26-gb404fb9-2 \
		-prefs=runtime.tools.xtensa-lx106-elf-gcc-1.20.0-26-gb404fb9-2.path=$(ESP8266_TOOLS)/xtensa-lx106-elf-gcc/1.20.0-26-gb404fb9-2 \
		-verbose \
		$<
