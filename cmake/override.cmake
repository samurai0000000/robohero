# cmake/override.cmake
#
# Copyright (C) 2026, Charles Chiou
#
# Force .c.o and .cpp.o extensions for object files so Xtensa linker script rules (*.c.o, *.cpp.o) match

set(CMAKE_C_OUTPUT_EXTENSION ".c.o")
set(CMAKE_CXX_OUTPUT_EXTENSION ".cpp.o")
set(CMAKE_ASM_OUTPUT_EXTENSION ".o")
