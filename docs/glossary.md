# Glossary

[← Back to index](README.md)

**ADC** — Analog-to-digital converter. Tuna uses the AVR's `ADC0` in 12-bit
single-ended mode to sample the microphone input.

**AC-coupled** — An input that passes only the changing (audio) part of a signal,
biased here to VDD/2 so the waveform swings around mid-scale.

**Bin** — A single output point of the FFT, corresponding to a frequency of
`k · SAMPLE_FREQ / FRAME_SIZE`.

**Cents** — A logarithmic unit of pitch; 100 cents = one equal-tempered semitone.
Tuna reports tuning error as ±50 cents around the nearest note.

**CMND** — Cumulative-Mean-Normalised Difference function, the core of the YIN
algorithm.

**DC removal** — Subtracting the mean (the 0 Hz component) of a frame before
analysis, so residual input bias does not leak into the low FFT bins.

**Decimation in time (DIT)** — The radix-2 FFT structure used by `fft()`, which
re-orders the input by bit-reversal and combines results in `log2(n)` butterfly
passes.

**DFP** — Device Family Pack. Microchip's `.atpack` (a zip) containing the
AVR64DD14 runtime and register headers, auto-downloaded at configure time.

**EMA** — Exponential Moving Average; the smoother's `α·new + (1−α)·old` blend that
steadies the cents readout for a held note.

**Event System (EVSYS)** — AVR peripheral interconnect. Tuna routes the TCA0
overflow directly to the ADC start trigger, so conversions start in hardware
without ISR jitter.

**FFT** — Fast Fourier Transform. Tuna's optional frequency-domain pitch engine; a
fixed-point in-place radix-2 implementation run as a real-input transform.

**Fixed point / Q15** — Integer arithmetic representing fractional values; in Q15
the range −32768..+32767 maps to −1.0..+1.0.

**`F_CPU`** — The CPU clock frequency the firmware is told to assume (24 MHz).
Drives the sample-rate timer and busy-wait delays; must match the real clock.

**Fundamental** — The lowest frequency of a periodic tone — the pitch we want.
Harmonics are integer multiples of it.

**HAL** — Hardware Abstraction Layer (`hal.c`), the only hardware-touching module
and the porting seam.

**HPS** — Harmonic Product Spectrum, the family of techniques behind the FFT
path's octave correction (`fundamental_divisor`).

**MAX7219** — The LED-driver chip that drives the 7-segment digits and bar graph,
bit-banged over three GPIOs.

**Nyquist frequency** — Half the sample rate (`SAMPLE_FREQ/2`); the highest
frequency that can be represented, and the cap on detectable pitch.

**Octave jump / error** — A half- or double-pitch mistake by the estimator. The
smoother vetoes these unless they persist for `OCTAVE_JUMP_FRAMES`.

**Parabolic interpolation** — Fitting a parabola to three points around a peak
(FFT) or dip (YIN) to estimate its true location between samples/bins.

**Pitch class** — One of the twelve semitone names within an octave (C, C#, …, H),
independent of octave.

**Spectral leakage** — Energy from a tone spreading into neighbouring FFT bins
when it does not fall exactly on a bin; reduced by windowing.

**TCA0** — A timer/counter peripheral; here the free-running timer that paces ADC
conversions at 4 × `SAMPLE_FREQ` (each group of 4 is averaged into one delivered
sample).

**Twiddle factor** — The complex exponential `exp(−j·2πk/N)` multiplied in at each
FFT butterfly, read from the shared `SINEWAVE` table.

**UPDI** — Unified Program and Debug Interface, the single-wire programming
interface used to flash AVR-DD parts.

**Window function** — A taper (Hann/Hamming/Blackman/rectangular) applied before
the FFT to reduce spectral leakage.

**YIN** — A time-domain autocorrelation pitch-detection algorithm (de Cheveigné &
Kawahara, 2002); Tuna's default pitch engine.
