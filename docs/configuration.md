# Compile-time configuration

[← Back to index](README.md)

`src/config.h` is the **single tuning point** for the firmware. Everything that
shapes behaviour — frame size, sample rate, pitch method, thresholds, window,
note naming, smoothing — is a macro here (or can be overridden from the build).
There are no runtime settings.

## Acquisition and framing

| Macro | Default | Meaning |
|---|---|---|
| `FRAME_SIZE` | 1024 | Samples per analysis frame; also the acquisition buffer length shared by both pitch methods. The FFT path additionally treats it as the transform length. **Must be a power of two** (the FFT and the derived `LOG2_FRAME_SIZE` both require it). |
| `LOG2_FRAME_SIZE` | `__builtin_ctz(FRAME_SIZE)` | Log2 of `FRAME_SIZE`, derived so the two cannot drift. `__builtin_ctz` of a power of two is its base-2 log; the compiler folds it to a constant. Used only in ordinary integer expressions (never in a `#if` or array bound). |
| `SAMPLE_FREQ` | `4096UL` | Rate in Hz of the frames delivered to analysis. The HAL paces the ADC at 4× this rate (`OVERSAMPLE_FACTOR` in `hal.c`) and averages each group of 4 conversions into one sample — a first-order anti-aliasing comb plus ADC noise reduction. Sets the TCA0 top value and scales every reported frequency; the Nyquist limit `SAMPLE_FREQ/2` caps detectable pitch. The 16-bit timer bounds the oversampled rate to ≥ 367 Hz, i.e. `SAMPLE_FREQ` ≥ 92 Hz. |
| `SILENCE_THRESHOLD` | 40 | Peak AC excursion (ADC counts; full scale ±2048 for the 12-bit single-ended result) below which a frame is treated as silence and the display blanks. |
| `SIGNAL_MIN_SAMPLES` | 8 | Number of samples per frame that must reach `SILENCE_THRESHOLD` before the frame counts as signal, so a single impulse (switch click) cannot open the gate. A real tone at threshold amplitude crosses hundreds of times per frame. |

## Pitch-detection method

| Macro | Meaning |
|---|---|
| `PITCH_METHOD_YIN` | Select the time-domain YIN autocorrelation estimator (the default if neither is defined). |
| `PITCH_METHOD_FFT` | Select the frequency-domain FFT peak picker. |

`config.h` defines `PITCH_METHOD_YIN` if neither is set, and `#error`s if **both**
are set. The default can be overridden from the build system (e.g.
`-DPITCH_METHOD_FFT`, which the CMake `PITCH_METHOD` option emits) so both paths
can be compiled in CI without editing the file.

## YIN parameters

| Macro | Default | Meaning |
|---|---|---|
| `YIN_THRESHOLD` | `0.15f` | Absolute threshold. The first dip in the cumulative-mean-normalised difference function below this is taken as the period. Typical 0.10–0.20; lower is stricter. |

(`YIN_W`, `YIN_TAU_MIN`, `YIN_TAU_MAX` are derived/fixed in `yin.c`, not in
`config.h`.)

## FFT parameters

These are unused when `PITCH_METHOD_YIN` is selected.

| Macro | Default | Meaning |
|---|---|---|
| `WINDOW_FUNCTION` | `WINDOW_HANNING` | Window applied before the FFT. Set to exactly one of `WINDOW_DIRICHLET`, `WINDOW_HANNING`, `WINDOW_HAMMING`, `WINDOW_BLACKMAN`. `window.c` selects the matching table and errors out if it is unset/unknown. Overridable from the build, e.g. `-DWINDOW_FUNCTION=WINDOW_BLACKMAN`. |
| `FFT_MAX_SUBHARMONIC` | 4 | Largest sub-multiple of the peak bin considered when correcting an octave error. |
| `FFT_HARMONIC_THRESHOLD_SHIFT` | 4 | Supporting-harmonic magnitude threshold is `peak >> shift`. Larger shift = lower threshold = recovers weaker fundamentals but is more permissive under noise. |

`WINDOW_FUNCTION` is a single-valued selector resolved with `#if/#elif`. The
note-naming choice (below) instead switches with `#ifdef`.

## Note naming / display

| Macro | Meaning |
|---|---|
| `ACCIDENTAL_SHARP` | Defined → name accidentals as sharps (`C#`, `D#`, …). Undefined → name them as flats. Switches both the `PITCH_CLASS` enum (`pitch.h`) and the note-letter tables in `display_note()`. (Either way, German `H` is used for English B.) |

## Frequency smoothing

| Macro | Default | Meaning |
|---|---|---|
| `SMOOTHING_ALPHA` | 0.5 | EMA weight (0..1) on the newest estimate while refining a held note. Lower steadies the cents needle at the cost of a slower response. |
| `OCTAVE_JUMP_FRAMES` | 2 | Consecutive frames an octave jump must persist before it is accepted as a real octave change rather than a transient half/double-pitch error. 2 rejects any single-frame glitch while keeping the lag on a genuine octave change to one frame (~250 ms). |

## Build-time overrides summary

Several `config.h` defaults can be overridden without editing the file, which is
how CI exercises multiple configurations:

| Setting | How to override | Notes |
|---|---|---|
| Pitch method | CMake `-DPITCH_METHOD=YIN` or `=FFT` (emits `-DPITCH_METHOD_<v>`) | `config.h` honours the predefined macro over its default. |
| Window function | `-DWINDOW_FUNCTION=WINDOW_BLACKMAN` | Only affects the FFT path. |
| CPU clock / sample math | `-DF_CPU=<hz>` | Must match the real clock set in `hal.c`. |
| `double` width | `-mdouble=64` | If FFT/pitch precision regresses vs the old XC8 build. |
