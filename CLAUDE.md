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

## Testing / CI

The DSP code (`fft.c`, `analysis.c`, `yin.c`) is plain integer/fixed-point C with no
AVR dependencies, so it is exercised by **host regression tests** under `test/` that
compile the real sources with the native compiler and check pitch detection on
synthetic signals (CTest):

```sh
cmake -S test -B test/build
cmake --build test/build
ctest --test-dir test/build --output-on-failure
```

GitHub Actions (`.github/workflows/ci.yml`) runs those host tests and cross-compiles the
firmware for the AVR64DD14 for **both** pitch methods (the build forwards
`-DPITCH_METHOD=YIN|FFT`, which `config.h` honours over its default). The device pack
downloads on the runner, so CI performs the on-target compile. Keep new DSP behaviour
covered by a `test/` case where practical.

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

Acquisition and analysis are **double-buffered and pipelined**: there are two
`int16_t[FFT_SIZE]` buffers (`acquisition_buffer_a`/`_b`, declared in `acquisition.h`).
While one buffer is analysed, the ADC fills the other in the background, so sampling
overlaps analysis/display instead of stalling it. Within a single frame the
signal-processing stages still operate **in place on that one buffer** (the pitch
estimators overwrite it), so keep that in mind before adding intermediate copies:

1. **acquisition** — `acquisition_start()` kicks off a background fill from the ADC and
   `acquisition_wait()` blocks (sleeping the CPU between samples) until it completes.
2. **window** — `window_apply_window()` applies the window selected in `config.h`.
3. **fft** — `fft_real()` does an in-place real FFT (fixed-point; see `fix_mpy` in
   `fft.c`).
4. **analysis** — `analysis_absolute()` converts to magnitudes, then
   `analysis_find_interpolated_peak_frequency()` returns the peak frequency (with
   sub-bin interpolation).
5. **display** — `segment_*` / `bargraph_*` push the result out through the MAX7219.

### Layers

- **HAL (`hal.c/.h`)** — the only hardware-touching module. Wraps ADC sampling, a
  timer-driven "sample counter" with a callback, the bit-banged MAX7219 pins
  (DIN/CLK/LOAD on PC1/PC2/PC3), an idle-sleep primitive (`hal_sleep_idle()`), and
  busy-wait delays. Port any retargeting through here. (Hardware SPI0/USART cannot drive
  the display: the board's clock trace is PC2, and no pin-mux on this part outputs a
  shift clock on PC2 while driving data on PC1.)
- **Driver (`max7219.c/.h`)** — register-level MAX7219 driver (register addresses are
  `#define`s); clocks each 16-bit frame out through the HAL pin setters.
- **Display (`segment.c/.h`, `bargraph.c/.h`)** — present numbers/letters/levels via the
  MAX7219 driver.
- **DSP (`fft.c`, `window.c`, `analysis.c`, `pitch.c`)** — `fft.c` is a fixed-point
  in-place complex FFT (`fft()`, plus the shared `fix_mpy`/`SINEWAVE`). `analysis.c`
  runs it as a **real-input FFT**: it packs the real signal into a half-size complex
  FFT and splits the result, so the FFT path costs ~half the transform work and an
  `FFT_SIZE/2` scratch buffer instead of a full imaginary array. `pitch.c` maps
  frequencies to musical pitch classes.

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
