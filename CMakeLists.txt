cmake_minimum_required(VERSION 3.20)

# The AVR toolchain file must be supplied on the command line *before* project():
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/avr-toolchain.cmake -DAVR_DFP=/path/to/AVR-Dd_DFP
#   cmake --build build
project(Tuna LANGUAGES C)

# ---------------------------------------------------------------------------
# Target hardware
# ---------------------------------------------------------------------------
set(MCU   avr64dd14)
set(F_CPU 24000000UL)   # internal high-frequency oscillator @ 24 MHz (see hal.c)

# The AVR64DD14 is a newer AVR-DD part whose runtime (crt, libdevice) and headers are
# not bundled with stock avr-gcc/avr-libc. They ship in Microchip's AVR-Dx Device
# Family Pack (an .atpack, which is just a zip). By default we auto-download and extract
# it at configure time; set -DAVR_DFP=/path/to/extracted/pack to use a local copy
# instead (e.g. offline builds).
set(AVR_DFP "" CACHE PATH "Path to an extracted AVR-Dx DFP (empty = auto-download)")
set(AVR_DFP_VERSION "2.8.343" CACHE STRING "AVR-Dx DFP version to fetch when AVR_DFP is unset")

if(NOT AVR_DFP)
    include(FetchContent)
    FetchContent_Declare(avr_dfp
        URL "https://packs.download.microchip.com/Microchip.AVR-Dx_DFP.${AVR_DFP_VERSION}.atpack"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(avr_dfp)
    set(AVR_DFP "${avr_dfp_SOURCE_DIR}")
    message(STATUS "Using auto-downloaded AVR-Dx DFP ${AVR_DFP_VERSION}: ${AVR_DFP}")
else()
    message(STATUS "Using local AVR-Dx DFP: ${AVR_DFP}")
endif()

if(NOT EXISTS "${AVR_DFP}/gcc/dev/${MCU}")
    message(FATAL_ERROR "AVR_DFP does not contain gcc/dev/${MCU} (got: ${AVR_DFP})")
endif()

set(MCU_FLAGS -mmcu=${MCU} -B "${AVR_DFP}/gcc/dev/${MCU}")

# ---------------------------------------------------------------------------
# Firmware
# ---------------------------------------------------------------------------
file(GLOB SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/Tuna/*.c")

add_executable(${PROJECT_NAME}.elf ${SOURCES})

target_include_directories(${PROJECT_NAME}.elf PRIVATE "${CMAKE_SOURCE_DIR}/Tuna")
target_compile_definitions(${PROJECT_NAME}.elf PRIVATE F_CPU=${F_CPU})
target_include_directories(${PROJECT_NAME}.elf SYSTEM PRIVATE "${AVR_DFP}/include")

target_compile_options(${PROJECT_NAME}.elf PRIVATE
    ${MCU_FLAGS}
    -Os -flto
    -ffunction-sections -fdata-sections
    -funsigned-char -funsigned-bitfields
    -Wall -Wextra
)

target_link_options(${PROJECT_NAME}.elf PRIVATE
    ${MCU_FLAGS}
    -Os -flto
    -Wl,--gc-sections
    -Wl,--relax
    -Wl,-Map=${PROJECT_NAME}.map
)

# analysis.c / pitch.c use the math library.
target_link_libraries(${PROJECT_NAME}.elf m)

# ---------------------------------------------------------------------------
# Artifacts: .hex for flashing + a size report
# ---------------------------------------------------------------------------
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${AVR_OBJCOPY} -O ihex -R .eeprom
            "$<TARGET_FILE:${PROJECT_NAME}.elf>" "${PROJECT_NAME}.hex"
    COMMAND ${AVR_SIZE} "$<TARGET_FILE:${PROJECT_NAME}.elf>"
    BYPRODUCTS "${PROJECT_NAME}.hex"
    VERBATIM
    COMMENT "Generating ${PROJECT_NAME}.hex and reporting size")

# ---------------------------------------------------------------------------
# Flashing (avrdude) — programmer to be filled in once hardware is on hand.
# Example programmers: pkobn_updi (Curiosity/Xplained on-board), serialupdi,
# snap_updi, pickit4_updi, xplainedmini_updi.
#   cmake -B build ... -DAVRDUDE_PROGRAMMER=serialupdi -DAVRDUDE_PORT=/dev/ttyUSB0
#   cmake --build build --target flash
# ---------------------------------------------------------------------------
set(AVRDUDE_PROGRAMMER "" CACHE STRING "avrdude -c programmer id (e.g. serialupdi, pkobn_updi)")
set(AVRDUDE_PORT       "usb" CACHE STRING "avrdude -P port (e.g. usb, /dev/ttyUSB0)")

if(NOT AVRDUDE_PROGRAMMER)
    message(STATUS "AVRDUDE_PROGRAMMER not set — the 'flash' target needs "
                   "-DAVRDUDE_PROGRAMMER=<id> once the programmer is known.")
endif()

add_custom_target(flash
    COMMAND ${AVRDUDE} -c "${AVRDUDE_PROGRAMMER}" -p ${MCU} -P "${AVRDUDE_PORT}"
            -U "flash:w:${PROJECT_NAME}.hex:i"
    DEPENDS ${PROJECT_NAME}.elf
    VERBATIM
    COMMENT "Flashing ${PROJECT_NAME}.hex via avrdude")
