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

The DSP and pure decision logic (`fft.c`, `analysis.c`, `yin.c`, `pitch.c`,
`smoothing.c`) is plain integer/fixed-point/floating C with no AVR dependencies, so it is
exercised by **host regression tests** under `test/` (`test_fft`, `test_yin`,
`test_pitch`, `test_smoothing`) that compile the real sources with the native compiler
and check pitch detection, note mapping and frequency smoothing on synthetic inputs
(CTest):

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
INIT -> ACQUISITION -> ANALYSIS -> DISPLAY -> (back to ACQUISITION)   [ERROR is a halt trap]
```

Acquisition and analysis are **double-buffered and pipelined**: `acquisition.c` owns two
`int16_t[FRAME_SIZE]` buffers internally and ping-pongs between them. While one buffer is
analysed, the ADC fills the other in the background, so sampling overlaps analysis/display
instead of stalling it. Within a single frame the signal-processing stages still operate
**in place on that one buffer** (the pitch estimators overwrite it), so keep that in mind
before adding intermediate copies:

1. **acquisition** — `acquisition_prime()` launches the first background fill at startup;
   thereafter `acquisition_collect()` blocks (sleeping the CPU between samples) until the
   in-flight fill completes, relaunches the next fill into the other buffer, and returns
   the just-filled frame for analysis. The buffer swap lives inside `acquisition.c`.
2. **analysis** — the frame is gated on input level (`signal_is_present()`), then the
   pitch method selected in `config.h` estimates the fundamental frequency:
   - **YIN** (default) — `yin_frequency()` (`yin.c`) does a time-domain autocorrelation
     estimate; no windowing/FFT involved.
   - **FFT** — `analysis_fft_frequency()` (`analysis.c`) removes DC, applies
     `window_apply_window()`, runs `fft()` (`fft.c`) as an in-place real-input transform
     (fixed-point; see `fix_mpy`), converts to magnitudes and returns the
     parabolically-interpolated peak frequency with HPS-style octave correction.

   `smooth_frequency()` (`smoothing.c`) then stabilises the per-frame estimate.
3. **display** — `pitch_from_frequency()` maps the frequency to a note, then `segment_*`
   / `bargraph_*` push the result out through the MAX7219.

### Layers

- **HAL (`hal.c/.h`)** — the only hardware-touching module. Wraps ADC sampling, a
  timer-driven "sample counter" with a callback, the bit-banged MAX7219 pins
  (DIN/CLK/LOAD on PC1/PC2/PC3), an idle-sleep primitive (`hal_sleep_idle()`), and
  busy-wait delays. Port any retargeting through here. (Hardware SPI0/USART cannot drive
  the display: the board's clock trace is PC2, and no pin-mux on this part outputs a
  shift clock on PC2 while driving data on PC1.)
- **Driver (`max7219.c/.h`)** — register-level MAX7219 driver (register addresses are
  `#define`s); clocks each 16-bit frame out through the HAL pin setters.
- **Display (`display.c/.h`, `segment.c/.h`, `bargraph.c/.h`)** — `segment.c`/`bargraph.c`
  render numbers/letters/levels and *stage* them into `display.c`, a 5-byte shadow of the
  MAX7219 digit registers. A single `display_flush()` then transmits only the digits that
  changed since the last frame, coalescing each frame's updates into one atomic burst.
- **DSP (`fft.c`, `window.c`, `analysis.c`, `yin.c`, `pitch.c`)** — `fft.c` is a
  fixed-point in-place complex FFT (`fft()`, plus the shared `fix_mpy`/`SINEWAVE`).
  `analysis.c` runs it as a **real-input FFT**: it packs the real signal into a half-size
  complex FFT and splits the result, so the FFT path costs ~half the transform work and
  an `FRAME_SIZE/2` scratch buffer instead of a full imaginary array. `yin.c` is the
  alternative time-domain (YIN autocorrelation) pitch estimator. `pitch.c` maps
  frequencies to musical pitch classes; `smoothing.c` stabilises the per-frame estimate
  (EMA plus octave-jump rejection).
- **Control (`control.c`, `main.c`)** — the cooperative state machine described above,
  plus the startup splash. The `ERROR`/`default` case is a defensive halt trap: it
  latches an "Er" indication and idles the CPU, reachable only if the state variable is
  corrupted.

### Compile-time configuration

`config.h` is the single tuning point. It defines `FRAME_SIZE` (1024, the analysis
frame / acquisition buffer length shared by both pitch methods; only the FFT path also
treats it as a transform length) with `LOG2_FRAME_SIZE` derived from it via
`__builtin_ctz` so the two cannot drift, plus `SAMPLE_FREQ`, the window function
(`WINDOW_HAMMING`), and the accidental convention (`ACCIDENTAL_SHARP` vs flat). The window
and note-naming choices switch behavior via `#ifdef` (e.g. the `PITCH_CLASS` enum in
`pitch.h`).

## Conventions

- Every module is a `.c`/`.h` pair using the same banner-comment section layout
  (INCLUDES / DEFINITIONS AND MACROS / TYPEDEFS / PROTOTYPES / etc.). Match this when
  adding or editing files.
- Each file starts with the Unlicense (public domain) header and a Doxygen-style
  `@file`/`@brief` comment block. New files should follow suit.
