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

## Regression detection (CI)

`check_regression.py` is the CI gate. It builds the **shipped** config
(`-Os -flto`) for both pitch methods, measures flash/RAM/cycles, and compares
them to `baseline.json`, applying two independent checks:

- **Relative regression** — each metric must stay within the baseline's
  tolerances (default flash/RAM 2%, cycles 5%). Catches "this change made the
  firmware bigger or the DSP slower." These checks are **skipped with a warning**
  when the runner's avr-gcc version differs from the one recorded in
  `baseline.json`, because the numbers legitimately shift with the toolchain —
  re-baseline instead (see below).
- **Absolute ceiling** — flash ≤ 64 KB, RAM ≤ 8 KB (the avr64dd14's limits).
  Toolchain-independent, so it is **always** enforced. The FFT path already sits
  at ~95% of SRAM, so this is a live guard.

It exits non-zero on any enforced failure, writes a table to the GitHub step
summary, and runs as the `benchmark` job in `.github/workflows/ci.yml`.

```sh
cd bench
./check_regression.py            # gate the shipped config (what CI runs)
./check_regression.py --no-run   # reuse the last out/results.tsv
```

### Updating the baseline

`baseline.json` is toolchain-specific. It is refreshed two ways:

- **Automatically on merge to `develop`.** The `update-baseline` job in
  `ci.yml` re-runs `--update` after each push to `develop` and commits the
  result back. Because the metrics are deterministic, it only commits when a
  merge actually moved flash/RAM/cycles (or the runner's avr-gcc changed), so
  the baseline always tracks the tip of `develop` and pull requests are gated
  against current reality. (The commit uses `GITHUB_TOKEN`, which does not
  retrigger workflows, so it cannot loop.)
- **Manually**, for an *intended* regression in a PR. A required, failing gate
  blocks the merge, so if a change deliberately grows the firmware or the DSP,
  bump the baseline in the same PR:

  ```sh
  cd bench
  ./check_regression.py --update   # rewrites baseline.json from a fresh run
  git add baseline.json && git commit -m "bench: re-baseline"
  ```

The committed baseline was captured with avr-gcc 7.3.0 — the version `apt`
installs on `ubuntu-latest`, so it matches the CI runner.

> **Trade-off of auto-refresh:** because the baseline follows `develop`, the
> gate catches any *single* change that regresses past tolerance, but not slow
> drift accumulated one sub-tolerance step at a time across many merges. The
> absolute flash/RAM ceilings are the backstop against that. If you'd rather
> catch cumulative drift, pin the baseline (remove the `update-baseline` job)
> and bump it deliberately.

> **Branch protection:** the auto-refresh pushes directly to `develop`. If
> `develop` forbids pushes from Actions, drop the `update-baseline` job and rely
> on the manual in-PR bump above.

## Files

- `run_bench.sh` — driver; sweeps the configs, writes `results.md` (full run) and `out/results.tsv` (machine-readable). `--configs`/`--methods` restrict the sweep.
- `check_regression.py` — CI gate: compares the shipped config to `baseline.json` (tolerances + chip ceilings). `--update` rewrites the baseline.
- `baseline.json` — committed reference numbers + tolerances + ceilings.
- `bench_dsp.c` — AVR firmware harness: one pitch pass, bracketed by `GPIOR0` markers.
- `simrun.c` — host program: runs a harness image under simavr, prints the bracketed cycle count.
- `gen_frame.py` → `bench_frame.h` — the fixed 220 Hz (A3) input frame, embedded so every build sees identical data.
- `results.md` — generated full-sweep table (committed as the reference run).

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
