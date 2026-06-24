/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
*/

/**
 * @file   config.h
 * @author Dennis Witzig
 * @date   2022-09-29
 * @brief  This module contains the configuration for Tuna.
 */

#ifndef CONFIG_H_
#define CONFIG_H_

/*---------------------------------------------------------------------------*/
/*                               INCLUDES                                    */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                         DEFINITIONS AND MACROS                            */
/*---------------------------------------------------------------------------*/
/* --- Acquisition / framing (shared by both pitch methods) --------------- */

/**
 * @brief Number of samples in one analysis frame. This is the acquisition
 *        buffer length that both pitch methods consume; only the FFT path
 *        treats it as a transform length, so it is named for the frame rather
 *        than for the FFT. Must be a power of two (the FFT path and the derived
 *        LOG2_FRAME_SIZE below both require it).
 */
#define FRAME_SIZE 1024

/**
 * @brief Sample frequency in Hz.
 *        Minimum is 367 Hz.
 */
#define SAMPLE_FREQ 4096UL

/**
 * @brief Peak input amplitude (in ADC counts, full scale +-2048 for the 12 bit
 *        single-ended result) below which a frame is treated as silence and the
 *        display is blanked instead of reporting a noise-driven note.
 */
#define SILENCE_THRESHOLD 40

/**
 * @brief Hop between successive analysis windows, in samples. Acquisition keeps
 *        a rolling FRAME_SIZE history that the ADC fills continuously; each
 *        analysis frame slides forward by HOP_SIZE and overlaps the previous one
 *        by FRAME_SIZE - HOP_SIZE samples. A new reading is therefore produced
 *        every HOP_SIZE / SAMPLE_FREQ seconds instead of once per full frame,
 *        which spends the compute that would otherwise idle between frames on a
 *        higher display update rate and lower latency.
 *
 *        Must be in 1 .. FRAME_SIZE. HOP_SIZE == FRAME_SIZE reproduces the old
 *        non-overlapping behaviour. The update rate cannot exceed what the
 *        selected pitch method can analyse within one hop period; below that the
 *        pipeline simply re-analyses the most recent window and drops the hops it
 *        could not keep up with (the cadence degrades gracefully towards one
 *        frame's worth of analysis time, never worse than the old behaviour).
 *        The default FRAME_SIZE/2 doubles the rate to ~8 Hz (125 ms at
 *        SAMPLE_FREQ=4096), which the YIN path - roughly half a frame of compute
 *        - comfortably sustains.
 */
#ifndef HOP_SIZE
#define HOP_SIZE (FRAME_SIZE / 2)
#endif

/* --- Pitch-detection method --------------------------------------------- */

/**
 * @brief Pitch-detection method. Define exactly one of the following. The two
 *        implementations are interchangeable (both take the acquisition buffer
 *        and return a frequency in Hz) so they can be benchmarked against each
 *        other by toggling this switch. The default below can also be overridden
 *        from the build system (e.g. -DPITCH_METHOD_FFT) so both paths can be
 *        compiled in CI without editing this file.
 *          PITCH_METHOD_YIN - time-domain YIN autocorrelation (default)
 *          PITCH_METHOD_FFT - frequency-domain FFT peak picking
 */
#if !defined(PITCH_METHOD_YIN) && !defined(PITCH_METHOD_FFT)
#define PITCH_METHOD_YIN
#endif

#if defined(PITCH_METHOD_YIN) == defined(PITCH_METHOD_FFT)
#error "config.h: define exactly one of PITCH_METHOD_YIN or PITCH_METHOD_FFT"
#endif

/* --- YIN pitch method --------------------------------------------------- */

/**
 * @brief YIN absolute threshold. The first dip in the cumulative-mean-normalized
 *        difference function below this value is taken as the period estimate.
 *        Typical range 0.10 - 0.20; lower is stricter.
 */
#define YIN_THRESHOLD 0.15f

/* --- FFT pitch method (unused when PITCH_METHOD_YIN is selected) --------- */

/**
 * @brief Log2 of FRAME_SIZE, the FFT's transform order. Derived from
 *        FRAME_SIZE so the two cannot drift apart; __builtin_ctz of a power of
 *        two yields its base-2 logarithm, and the compiler folds it to a
 *        constant (it is only ever used in ordinary integer expressions, never
 *        in a #if or array bound).
 */
#define LOG2_FRAME_SIZE __builtin_ctz(FRAME_SIZE)

/**
 * @brief Window function applied before the FFT. Set WINDOW_FUNCTION to exactly
 *        one of the WINDOW_* identifiers below; window.c selects the matching
 *        coefficient table and errors out if it is unset or unknown. (The
 *        window is an FFT-path concept - the YIN method does not window its
 *        input.) The build system can override the default by predefining
 *        WINDOW_FUNCTION, e.g. -DWINDOW_FUNCTION=WINDOW_BLACKMAN.
 */
#define WINDOW_DIRICHLET 1   /* rectangular - no taper */
#define WINDOW_HANNING   2
#define WINDOW_HAMMING   3
#define WINDOW_BLACKMAN  4

#ifndef WINDOW_FUNCTION
#define WINDOW_FUNCTION WINDOW_HANNING
#endif

/**
 * @brief FFT pitch method, octave correction. The strongest spectral bin is
 *        often a harmonic rather than the fundamental. The detector steps down
 *        to the lowest sub-multiple (peak bin / m, for m up to
 *        FFT_MAX_SUBHARMONIC) whose harmonic series is actually present in the
 *        spectrum, requiring each supporting harmonic bin to reach at least
 *        (peak >> FFT_HARMONIC_THRESHOLD_SHIFT) in magnitude. A larger shift is
 *        a lower threshold: it recovers weaker/missing fundamentals but is more
 *        permissive under noise. The fundamental is then read from the strong
 *        peak divided by m, giving m times finer absolute resolution than
 *        interpolating the low fundamental bin directly.
 */
#define FFT_MAX_SUBHARMONIC 4
#define FFT_HARMONIC_THRESHOLD_SHIFT 4

/* --- Note naming / display ---------------------------------------------- */

/**
 * @brief Note half step convention to use.
 */
#define ACCIDENTAL_SHARP

/* --- Frequency smoothing ------------------------------------------------ */

/**
 * @brief Exponential-moving-average weight applied to the newest frequency
 *        estimate while refining the reading of a held note (0..1). Lower values
 *        steady the cents needle at the cost of a slower response.
 */
#define SMOOTHING_ALPHA 0.5

/**
 * @brief Number of consecutive frames an octave jump must persist before it is
 *        accepted as a real octave change rather than a transient half/double-
 *        pitch error from the pitch estimator.
 */
#define OCTAVE_JUMP_FRAMES 3

/*---------------------------------------------------------------------------*/
/*                         TYPEDEFS AND STRUCTURES                           */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                            GLOBAL VARIABLES                               */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                           FUNCTION PROTOTYPES                             */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/

#endif /* CONFIG_H_ */
