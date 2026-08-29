# CMakeLists.txt
#
# Copyright (C) 2026, Charles Chiou
#

cmake_minimum_required(VERSION 3.5)

# Set toolchain and platform paths
if(NOT DEFINED ARDUINO15_ROOT)
    set(ARDUINO15_ROOT "$ENV{HOME}/.arduino15")
endif()

if(NOT DEFINED ARDUINO_LIBS_ROOT)
    set(ARDUINO_LIBS_ROOT "$ENV{HOME}/Arduino/libraries")
endif()

set(XTENSA_GCC_ROOT "${ARDUINO15_ROOT}/packages/esp8266/tools/xtensa-lx106-elf-gcc/1.20.0-26-gb404fb9-2")
set(ESP8266_ROOT "${ARDUINO15_ROOT}/packages/esp8266/hardware/esp8266/2.3.0")

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR xtensa)

set(CMAKE_C_COMPILER "${XTENSA_GCC_ROOT}/bin/xtensa-lx106-elf-gcc")
set(CMAKE_CXX_COMPILER "${XTENSA_GCC_ROOT}/bin/xtensa-lx106-elf-g++")
set(CMAKE_AR "${XTENSA_GCC_ROOT}/bin/xtensa-lx106-elf-ar")
set(CMAKE_RANLIB "${XTENSA_GCC_ROOT}/bin/xtensa-lx106-elf-ranlib")

set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CMAKE_CURRENT_LIST_DIR}/cmake/override.cmake")
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

project(robohero C CXX ASM)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler defines and common flags
add_definitions(
    -DF_CPU=80000000L
    -DLWIP_OPEN_SRC
    -DARDUINO=10819
    -DARDUINO_ESP8266_GENERIC
    -DARDUINO_ARCH_ESP8266
    -DARDUINO_BOARD="ESP8266_GENERIC"
    -DESP8266
    -D__ets__
    -DICACHE_FLASH
    -U__STRICT_ANSI__
)

# User configuration overrides (from CMake args or environment variables)
if(DEFINED AP_PASSWORD)
    add_definitions(-DAP_PASSWORD=\"${AP_PASSWORD}\")
elseif(DEFINED ENV{AP_PASSWORD})
    add_definitions(-DAP_PASSWORD=\"$ENV{AP_PASSWORD}\")
endif()

if(DEFINED CLIENT_SSID)
    add_definitions(-DCLIENT_SSID=\"${CLIENT_SSID}\")
elseif(DEFINED ENV{CLIENT_SSID})
    add_definitions(-DCLIENT_SSID=\"$ENV{CLIENT_SSID}\")
endif()

if(DEFINED CLIENT_PASSWORD)
    add_definitions(-DCLIENT_PASSWORD=\"${CLIENT_PASSWORD}\")
elseif(DEFINED ENV{CLIENT_PASSWORD})
    add_definitions(-DCLIENT_PASSWORD=\"$ENV{CLIENT_PASSWORD}\")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Os -g -Wpointer-arith -Wno-implicit-function-declaration -Wl,-EL -fno-inline-functions -nostdlib -mlongcalls -mtext-section-literals -falign-functions=4 -std=gnu99 -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -g -mlongcalls -mtext-section-literals -fno-exceptions -fno-rtti -falign-functions=4 -std=c++11 -ffunction-sections -fdata-sections")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -g -x assembler-with-cpp -mlongcalls")

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ESP8266_ROOT}/tools/sdk/include
    ${ESP8266_ROOT}/tools/sdk/lwip/include
    ${ESP8266_ROOT}/cores/esp8266
    ${ESP8266_ROOT}/variants/generic
    ${ESP8266_ROOT}/libraries/Wire
    ${ESP8266_ROOT}/libraries/Servo/src
    ${ESP8266_ROOT}/libraries/ESP8266WiFi/src
    ${ESP8266_ROOT}/libraries/ESP8266WebServer/src
    ${ESP8266_ROOT}/libraries/EEPROM
    ${ESP8266_ROOT}/libraries/Ticker
    ${ARDUINO_LIBS_ROOT}/Adafruit_PWM_Servo_Driver_Library
)

# ESP8266 Core Sources
file(GLOB_RECURSE ESP8266_CORE_SRCS
    "${ESP8266_ROOT}/cores/esp8266/*.c"
    "${ESP8266_ROOT}/cores/esp8266/*.cpp"
    "${ESP8266_ROOT}/cores/esp8266/*.S"
)
list(FILTER ESP8266_CORE_SRCS EXCLUDE REGEX ".*sigma_delta.*")

# Library Sources
set(ESP8266_LIB_SRCS
    ${ESP8266_ROOT}/libraries/Wire/Wire.cpp
    ${ESP8266_ROOT}/libraries/Servo/src/esp8266/Servo.cpp
    ${ESP8266_ROOT}/libraries/ESP8266WiFi/src/ESP8266WiFi.cpp
    ${ESP8266_ROOT}/libraries/ESP8266WiFi/src/ESP8266WiFiAP.cpp
    ${ESP8266_ROOT}/libraries/ESP8266WiFi/src/ESP8266WiFiGeneric.cpp
    ${ESP8266_ROOT}/libraries/ESP8266WiFi/src/ESP8266WiFiMulti.cpp
    ${ESP8266_ROOT}/libraries/ESP8266WiFi/src/ESP8266WiFiSTA.cpp
    ${ESP8266_ROOT}/libraries/ESP8266WiFi/src/ESP8266WiFiScan.cpp
    ${ESP8266_ROOT}/libraries/ESP8266WiFi/src/WiFiClient.cpp
    ${ESP8266_ROOT}/libraries/ESP8266WiFi/src/WiFiClientSecure.cpp
    ${ESP8266_ROOT}/libraries/ESP8266WiFi/src/WiFiServer.cpp
    ${ESP8266_ROOT}/libraries/ESP8266WiFi/src/WiFiUdp.cpp
    ${ESP8266_ROOT}/libraries/ESP8266WebServer/src/ESP8266WebServer.cpp
    ${ESP8266_ROOT}/libraries/ESP8266WebServer/src/Parsing.cpp
    ${ESP8266_ROOT}/libraries/EEPROM/EEPROM.cpp
    ${ESP8266_ROOT}/libraries/Ticker/Ticker.cpp
    ${ARDUINO_LIBS_ROOT}/Adafruit_PWM_Servo_Driver_Library/Adafruit_PWMServoDriver.cpp
)

# Application Sources (.cxx)
set(ROBOHERO_CXX_SRCS
    Main.cxx
    RoboHeroApp.cxx
    RoboHeroMotions.cxx
    RoboHeroServo.cxx
    RoboHeroWeb.cxx
)

# Linker settings
set(ESP8266_SDK_LIB "${ESP8266_ROOT}/tools/sdk/lib")
set(ESP8266_SDK_LD "${ESP8266_ROOT}/tools/sdk/ld")
set(LINKER_SCRIPT "eagle.flash.2m.ld")

set(CMAKE_EXE_LINKER_FLAGS
    "-g -w -Os -nostdlib -Wl,--no-check-sections -u call_user_start -Wl,-static -L${ESP8266_SDK_LIB} -L${ESP8266_SDK_LD} -T${LINKER_SCRIPT} -Wl,--gc-sections -Wl,-wrap,system_restart_local -Wl,-wrap,register_chipv6_phy"
)

add_executable(robohero
    ${ROBOHERO_CXX_SRCS}
    ${ESP8266_CORE_SRCS}
    ${ESP8266_LIB_SRCS}
)

target_link_libraries(robohero
    "-Wl,--start-group"
    m
    gcc
    hal
    phy
    pp
    net80211
    wpa
    crypto
    main
    wps
    axtls
    smartconfig
    mesh
    wpa2
    lwip_gcc
    stdc++
    "-Wl,--end-group"
)
