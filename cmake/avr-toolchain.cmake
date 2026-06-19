# AVR cross-compilation toolchain for CMake.
#
# Usage:
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/avr-toolchain.cmake [-DAVR_DFP=/path/to/AVR-Dd_DFP]
#
# Requires the AVR GNU toolchain on PATH: avr-gcc, avr-libc, binutils-avr
# (Fedora: `sudo dnf install avr-gcc avr-libc avr-binutils avrdude`).

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR avr)

find_program(AVR_GCC     NAMES avr-gcc)
find_program(AVR_OBJCOPY NAMES avr-objcopy)
find_program(AVR_SIZE    NAMES avr-size)
find_program(AVRDUDE     NAMES avrdude)

if(NOT AVR_GCC)
    message(FATAL_ERROR
        "avr-gcc not found on PATH. Install the AVR GNU toolchain "
        "(avr-gcc, avr-libc, binutils-avr).")
endif()

set(CMAKE_C_COMPILER   "${AVR_GCC}")
set(CMAKE_ASM_COMPILER "${AVR_GCC}")

# Bare-metal target: CMake's compiler check cannot link a hosted executable,
# so probe with a static library instead.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Look for programs on the host, but headers/libraries only in the AVR sysroot.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
