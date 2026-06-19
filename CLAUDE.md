# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

Tuna is embedded C firmware for a chromatic instrument tuner. It samples audio from an
ADC, runs a fixed-point FFT, finds the dominant frequency, and shows the result on a
7-segment display and an LED bar graph driven by a MAX7219.

Target: **AVR64DD14** microcontroller, 24 MHz internal oscillator (configured in
`hal.c` via `CLKCTRL`). The chip is clocked and `F_CPU` defined to match — keep both in
sync if the clock changes.

## Building

The build is **CMake + the open AVR GNU toolchain** (avr-gcc, avr-libc, binutils-avr,
avrdude), which is cross-platform and the only supported build. The project was migrated
off Microchip Studio / XC8; no `.atsln`/`.cproj` remain.

```sh
# Fedora: sudo dnf install cmake avr-gcc avr-libc avr-binutils avrdude
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/avr-toolchain.cmake
cmake --build build          # produces build/Tuna.elf and build/Tuna.hex + size report
cmake --build build --target flash   # needs -DAVRDUDE_PROGRAMMER=<id> (set once HW is known)
```

Key points:
- **All five packages are required** — in particular `avr-libc` provides `<stdint.h>`,
  `<avr/io.h>`, etc. `avr-gcc` alone is not enough (you'll get `fatal error: stdint.h`).
- **Device pack is automatic.** The AVR64DD14 runtime (crt/libdevice) and register
  headers are not bundled with avr-gcc/avr-libc; they live in Microchip's AVR-Dx Device
  Family Pack. CMake auto-downloads and extracts it at configure time via FetchContent
  (`build/_deps/avr_dfp-src`), pinned by `AVR_DFP_VERSION` in `CMakeLists.txt`. For an
  offline build, pass `-DAVR_DFP=/path/to/extracted/pack` to use a local copy instead.
- The compiler is mainline **avr-gcc** (XC8 for AVR is a rebranded avr-gcc with
  optimization gated behind a license; mainline removes that). clang's AVR backend is
  experimental and not used.
- Portability note: `<avr/io.h>` is used instead of XC8's `<xc.h>`. `double` is left at
  avr-gcc's default size; pass `-mdouble=64` if frequency-analysis precision regresses
  versus the old XC8 build.

Build artifacts (`*.elf`, `*.hex`, `*.map`, `build/`, `Debug/`, `Release/`) are
gitignored.

## Layout

- `src/` — all firmware sources; each module is a `.c` with its `.h` alongside.
- `cmake/avr-toolchain.cmake` — the AVR cross-compile toolchain file.
- `CMakeLists.txt` — top-level build (file paths below are relative to `src/`).

## Architecture

The program is a cooperative state machine, not an RTOS. `main()` calls `control()` in a
tight loop forever; `control()` (`control.c`) advances a single state variable through:

```
INIT -> ACQUISITION -> ANALYSIS -> DISPLAY -> (back to ACQUISITION)   [ERROR is a trap state]
```

The signal-processing pipeline operates **in place on one shared buffer**,
`acquisition_buffer` (`int16_t[FFT_SIZE]`, declared in `acquisition.h`). This single
buffer is reused across every stage to fit AVR RAM limits — keep that in mind before
adding intermediate copies:

1. **acquisition** — `acquisition_fill_buffer()` fills the buffer from the ADC.
2. **window** — `window_apply_window()` applies the window selected in `config.h`.
3. **fft** — `fft_real()` does an in-place real FFT (fixed-point; see `fix_mpy` in
   `fft.c`).
4. **analysis** — `analysis_absolute()` converts to magnitudes, then
   `analysis_find_interpolated_peak_frequency()` returns the peak frequency (with
   sub-bin interpolation).
5. **display** — `segment_*` / `bargraph_*` push the result out through the MAX7219.

### Layers

- **HAL (`hal.c/.h`)** — the only hardware-touching module. Wraps ADC sampling, a
  timer-driven "sample counter" with a callback, bit-banged MAX7219 pins
  (DIN/CLK/LOAD), and busy-wait delays. Port any retargeting through here.
- **Driver (`max7219.c/.h`)** — register-level MAX7219 driver (register addresses are
  `#define`s); built on top of the HAL pin setters.
- **Display (`segment.c/.h`, `bargraph.c/.h`)** — present numbers/letters/levels via the
  MAX7219 driver.
- **DSP (`fft.c`, `fft8.c`, `window.c`, `analysis.c`, `pitch.c`)** — `fft8` is an
  `int8_t` variant of the `int16_t` `fft`. `pitch.c` maps frequencies to musical pitch
  classes.

### Compile-time configuration

`config.h` is the single tuning point. It defines `FFT_SIZE` (1024) and the matching
`LOG2_FFT_SIZE` (10) — **keep these consistent** — plus `SAMPLE_FREQ`, the window
function (`HAMMING`), and the accidental convention (`SHARP` vs flat). The window and
note-naming choices switch behavior via `#ifdef` (e.g. the `PITCH_CLASS` enum in
`pitch.h`).

## Conventions

- Every module is a `.c`/`.h` pair using the same banner-comment section layout
  (INCLUDES / DEFINITIONS AND MACROS / TYPEDEFS / PROTOTYPES / etc.). Match this when
  adding or editing files.
- Each file starts with the Unlicense (public domain) header and a Doxygen-style
  `@file`/`@brief` comment block. New files should follow suit.
