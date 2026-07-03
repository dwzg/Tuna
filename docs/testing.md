# Testing

[← Back to index](README.md)

## Strategy

The DSP and pure decision logic — `fft.c`, `spectral.c`, `yin.c`, `pitch.c`,
`smoothing.c` — is plain integer/fixed-point/floating C with **no AVR
dependencies**. It therefore runs **bit-for-bit identically** on the host and on
the target, and is covered by **host regression tests** under `test/` that compile
the real `src/` sources with the native compiler. Hardware-touching code
(`hal.c`, `max7219.c`, acquisition, control) is not unit-tested; it is exercised
only by the CI cross-compile.

The test project is a **standalone** CMake project — do **not** pass it the AVR
toolchain file.

## Running the tests

```sh
cmake -S test -B test/build
cmake --build test/build
ctest --test-dir test/build --output-on-failure
```

Passing `-DTUNA_SANITIZE=ON` at configure time additionally builds the tests
with AddressSanitizer and UndefinedBehaviorSanitizer, so out-of-bounds buffer
accesses and arithmetic UB in the DSP code fail the run even when the computed
result happens to look right. CI enables this.

## Test programs

| Test | Sources under test | What it checks |
|---|---|---|
| `test_yin` | `yin.c` | Builds synthetic harmonic tones at known fundamentals and checks `yin_frequency()` recovers them. (YIN is the `config.h` default, so no override is needed.) |
| `test_fft` | `spectral.c` + `fft.c` (built with `PITCH_METHOD_FFT`) | Real-input FFT frequency accuracy on pure tones, plus HPS-style octave correction on weak/missing-fundamental tones. Uses a simple Hamming setup. |
| `bench_window` | `spectral.c` + `fft.c` (`PITCH_METHOD_FFT`) | Benchmarks FFT-path pitch accuracy across all window functions over a corpus of synthetic tones (pure, harmonic-rich, weak-fundamental; clean and noisy), reporting cents error (mean / p95 / max) and guarding that the shipped default window stays accurate. |
| `test_pitch` | `pitch.c` | `pitch_from_frequency()` maps known frequencies to the right pitch class/octave with the expected cents, and rejects out-of-range input. Assumes the default `ACCIDENTAL_SHARP` convention. |
| `test_smoothing` | `smoothing.c` | `smooth_frequency()`: the silence reset, EMA hold, transient octave-error rejection (with eventual give-in after `OCTAVE_JUMP_FRAMES`), and the snap-through on a genuine note change. Assumes `SMOOTHING_ALPHA=0.5`, `OCTAVE_JUMP_FRAMES=3`. |

The test build uses `-std=c11 -O2 -Wall -Wextra` and links `libm`. The FFT-path
targets are compiled with `PITCH_METHOD_FFT` defined so `spectral_frequency()` is
actually built (it is `#ifdef`-guarded out otherwise).

## Adding tests

Keep new DSP behaviour covered by a `test/` case where practical. To add a test:

1. Write `test/test_<name>.c` exercising the relevant `src/` source(s) directly.
2. In `test/CMakeLists.txt`, add an `add_executable` + `target_include_directories`
   (pointing at `../src`) + `target_link_libraries(... m)` + `add_test(...)`,
   mirroring the existing entries. Define `PITCH_METHOD_FFT` if the code under test
   is FFT-path only.

## How CI uses the tests

The `host-tests` CI job runs the three commands above (configuring with
`-DTUNA_SANITIZE=ON`) on `ubuntu-latest` with the native compiler — no AVR
toolchain. The separate
`firmware` job then cross-compiles for the AVR64DD14 for both `YIN` and `FFT`, so
each commit gets both a **behavioural** check (host tests) and a **both-paths
compile** check on the real toolchain. See
[Building, flashing and CI](building.md).
