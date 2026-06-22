# Compiler-optimization benchmark

This directory measures how compiler optimization choices affect Tuna along the
two axes that matter on the AVR64DD14: **how much flash/RAM the firmware costs**
and **how many CPU cycles the DSP hot path burns**. It exists to justify the
optimization flags in the top-level `CMakeLists.txt` (`-Os -flto`) with numbers
rather than folklore.

## What it measures

For every (optimization config × pitch method) it reports:

| Metric        | How                                                                 | Fidelity |
|---------------|---------------------------------------------------------------------|----------|
| Flash, RAM    | the **real firmware** cross-compiled for `avr64dd14`, via `avr-size` | exact for the target |
| DSP cycles    | one `yin_frequency()` / `spectral_frequency()` pass over a fixed frame, counted under **simavr** | relative proxy (see caveat) |

Flash = `text+data`, RAM = `data+bss`. The DSP-cycle figure is bracketed
precisely around the pitch call — frame setup and libm startup are excluded.

The pitch estimators overwrite their input buffer in place, so a single pass is
the natural unit; the result is deterministic, so one pass is all that's needed.

## The simavr caveat (important)

simavr (1.6) has **no AVR-Dx core**, so the cycle counts are taken on an
`atmega1284p` — a classic **AVRe+** core with enough SRAM for the ~3 KB DSP
working set. The AVR64DD14 runs the newer **AVRxt** core, which is a little
faster on some instructions, so absolute cycle counts here run somewhat high
versus real hardware. They are a **consistent basis for comparing optimization
configs**, not an absolute timing of the Dx. The flash/RAM numbers, by contrast,
*are* measured on the true target and are exact.

The **frame budget** annotation (6,000,000 cycles = 250 ms at 24 MHz, i.e.
`FRAME_SIZE/SAMPLE_FREQ`) is the time one frame takes to acquire. Analysis is
double-buffered against acquisition, so exceeding the budget doesn't break the
tuner — it just caps the display refresh at the analysis rate instead of the
frame rate.

## Running

```sh
# One-time: configure the CMake build so the AVR-Dx device pack is downloaded
# (run_bench.sh reuses build/_deps/avr_dfp-src), or pass AVR_DFP=/path explicitly.
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/avr-toolchain.cmake

cd bench
./run_bench.sh        # writes results.md
```

Needs `avr-gcc`, `avr-size`, `libsimavr` (Fedora: `simavr simavr-devel`;
Debian/Ubuntu: `simavr libsimavr-dev`), and an extracted AVR-Dx DFP.

## Files

- `run_bench.sh` — driver; sweeps the configs and writes `results.md`.
- `bench_dsp.c` — AVR firmware harness: one pitch pass, bracketed by `GPIOR0` markers.
- `simrun.c` — host program: runs a harness image under simavr, prints the bracketed cycle count.
- `gen_frame.py` → `bench_frame.h` — the fixed 220 Hz (A3) input frame, embedded so every build sees identical data.
- `results.md` — generated table (committed as the reference run).

## Key findings (reference run: avr-gcc 7.3.0)

Numbers are toolchain-specific; a newer avr-gcc shifts the absolutes but not the
trends. See `results.md` for the full table.

1. **`-Os -flto` (the shipped config) is the sweet spot.** It produces the
   smallest YIN firmware and is simultaneously the *fastest* YIN config measured
   — optimizing for size also won here because the hot loops fit better and LTO
   inlines across modules. No reason to change it.
2. **`-O0` is disqualifying.** It roughly *triples* DSP cycle cost (YIN ~2.7× the
   `-Os -flto` time) and bloats flash. Never ship it.
3. **`-O3` is a bad trade on this target.** `-O3` / `-O3 -flto` inflate flash
   massively (YIN `-O3 -flto` is ~2.4× the flash of `-Os -flto`) for no
   meaningful speedup — sometimes a slight *regression* — because aggressive
   unrolling/inlining hurts on a small core with a tiny I-cache-less pipeline.
4. **FFT is the cheaper compute path, despite YIN being the default.** Across
   every config the FFT pitch path runs in fewer DSP cycles than YIN (its
   `O(N log N)` beats YIN's `O(N²)` difference function at `FRAME_SIZE=1024`),
   at the cost of more flash and RAM (it pulls in `fft.c`, `window.c` and
   `log()`). If analysis latency ever becomes the constraint, FFT is the lever.
