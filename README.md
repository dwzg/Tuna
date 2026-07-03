# Tuna

Embedded C firmware for a chromatic instrument tuner built around the
Microchip **AVR64DD14**. It samples audio from the on-chip ADC, estimates the
fundamental frequency (time-domain YIN by default, or a fixed-point real-input
FFT), maps it to the nearest equal-tempered note, and shows the result on a
two-digit 7-segment display plus a 20-LED bar graph "tuning needle", all driven
through a MAX7219.

## Quick start

The build is CMake + the open AVR GNU toolchain (avr-gcc, avr-libc,
binutils-avr, avrdude). The AVR-Dx device pack is fetched automatically at
configure time.

```sh
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/avr-toolchain.cmake
cmake --build build          # build/Tuna.elf + build/Tuna.hex + size report
```

The DSP and decision logic is plain C with no AVR dependencies and is covered
by host regression tests:

```sh
cmake -S test -B test/build
cmake --build test/build
ctest --test-dir test/build --output-on-failure
```

## Documentation

The full technical reference lives in [`docs/`](docs/README.md): hardware,
architecture, the signal-processing chain, the display subsystem, every
`config.h` knob, and the CI/benchmark setup. Performance benchmarks and the
regression gate are under [`bench/`](bench/README.md).

## License

Released into the public domain under the [Unlicense](LICENSE).
