# RoboHero

<!-- robohero-version: 2.1.16 -->
**Version: 2.1.16**

FreeRTOS firmware for the 17-servo TT Robotix RoboHero, along with companion host CLI, Android mobile app, and Web interface.

`IDF_PATH` is the `ESP8266_RTOS_SDK` git submodule at the repo root
(branch `release/v3.4`, currently `v3.4-114-g57350732`). The root
`Makefile` sets `IDF_PATH` to that directory if it is unset.

## Build

Toolchain: xtensa-lx106-elf gcc 8.4.0. The Makefile prepends
`$HOME/esp/xtensa-lx106-elf/bin` when that compiler exists.

The first `make` (and `make` after `distclean`) copies `sdkconfig.defaults`
to `sdkconfig` so configuration stays non-interactive. `make menuconfig`
is optional and needs `gperf flex bison` on the host.

Python packages used by the SDK (once per machine):

```
python3 -m pip install --user --break-system-packages \
    -r ESP8266_RTOS_SDK/requirements.txt
```

```
git submodule update --init --recursive
make
make flash ESPPORT=/dev/ttyUSB0
make monitor ESPPORT=/dev/ttyUSB0
```

Flash: 2 MB, DIO, 40 MHz, 80 MHz CPU. Serial 115200.
GPIO12 software PWM requires the NMI watchdog off (`sdkconfig.defaults`).

First boot uses factory NVS (zero trims, default AP password `12345678`).
