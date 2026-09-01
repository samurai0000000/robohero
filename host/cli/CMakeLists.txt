#
# CMakeLists.txt
#
# Copyright (C) 2026, Charles Chiou
#

cmake_minimum_required(VERSION 3.16)

# Extract version from root CMakeLists.txt
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../../CMakeLists.txt" ROOT_CMAKELISTS)
string(REGEX MATCH "project\\([ \t\r\n]*robohero[ \t\r\n]+VERSION[ \t\r\n]+([0-9]+)\\.([0-9]+)\\.([0-9]+)" _VER_MATCH "${ROOT_CMAKELISTS}")
if(_VER_MATCH)
    set(PROJ_VER_MAJOR "${CMAKE_MATCH_1}")
    set(PROJ_VER_MINOR "${CMAKE_MATCH_2}")
    set(PROJ_VER_PATCH "${CMAKE_MATCH_3}")
    set(PROJ_VER "${PROJ_VER_MAJOR}.${PROJ_VER_MINOR}.${PROJ_VER_PATCH}")
else()
    set(PROJ_VER_MAJOR "1")
    set(PROJ_VER_MINOR "0")
    set(PROJ_VER_PATCH "0")
    set(PROJ_VER "1.0.0")
endif()

project(robohero_cli VERSION ${PROJ_VER} LANGUAGES CXX C)

set(PROJECT_VERSION_MAJOR ${PROJ_VER_MAJOR})
set(PROJECT_VERSION_MINOR ${PROJ_VER_MINOR})
set(PROJECT_VERSION_PATCH ${PROJ_VER_PATCH})
set(PROJECT_VERSION ${PROJ_VER})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -O2)

set(WHOAMI_OUTPUT)
execute_process(
    COMMAND whoami
    OUTPUT_VARIABLE WHOAMI_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(HOSTNAME_OUTPUT)
execute_process(
    COMMAND hostname
    OUTPUT_VARIABLE HOSTNAME_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(DATE_OUTPUT)
execute_process(
    COMMAND date "+%Y-%m-%d %H:%M:%S"
    OUTPUT_VARIABLE DATE_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.h
    @ONLY
)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/version.h
    @ONLY
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCONFIGPP REQUIRED libconfig++)
pkg_check_modules(LIBMOSQUITTO REQUIRED libmosquitto)
pkg_check_modules(NCURSES REQUIRED ncurses)

find_package(Threads REQUIRED)

set(HOST_SRCS
    Configuration.cxx
    Mqtt.cxx
    RoboHeroHost.cxx
    main.cxx
)

add_executable(robohero_cli ${HOST_SRCS})

target_include_directories(robohero_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../main
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/include
    ${LIBCONFIGPP_INCLUDE_DIRS}
    ${LIBMOSQUITTO_INCLUDE_DIRS}
    ${NCURSES_INCLUDE_DIRS}
)

target_link_libraries(robohero_cli PRIVATE
    ${LIBCONFIGPP_LIBRARIES}
    ${LIBMOSQUITTO_LIBRARIES}
    ${NCURSES_LIBRARIES}
    Threads::Threads
)
