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
 * @file   spectral.c
 * @author Dennis Witzig
 * @date   2022-10-21
 * @brief  This module contains the code to analyze the audio spectrum and
 *         determine the main frequency of the signal via an FFT.
 */

/*---------------------------------------------------------------------------*/
/*                               INCLUDES                                    */
/*---------------------------------------------------------------------------*/
#include <stdint.h>
#include "config.h"
#include "spectral.h"

#ifdef PITCH_METHOD_FFT

#include <math.h>
#include "window.h"
#include "fft.h"
#include "profile.h"

/*---------------------------------------------------------------------------*/
/*                         DEFINITIONS AND MACROS                            */
/*---------------------------------------------------------------------------*/
/**
 * @brief Number of usable spectral bins (0 .. Nyquist) of a real FRAME_SIZE FFT.
 */
#define SPECTRUM_BINS (FRAME_SIZE / 2)

/*---------------------------------------------------------------------------*/
/*                               PROTOTYPES                                  */
/*---------------------------------------------------------------------------*/
static int16_t real_bin_mag(int16_t ar, int16_t ai, int16_t br, int16_t bi, uint16_t k);
static int16_t real_dc_nyquist_mag(int16_t zr, int16_t zi, int8_t sign);
static uint8_t fundamental_divisor(const int16_t spectrum[], uint16_t peak_bin, int16_t peak);

/*---------------------------------------------------------------------------*/
/*                            LOCAL VARIABLES                                */
/*---------------------------------------------------------------------------*/
/*
 * Imaginary half of the N/2-point complex FFT used by the real-input transform
 * (the real half is the caller's buffer). Only FRAME_SIZE/2 entries are needed,
 * half the storage of a full complex imaginary buffer.
 */
static int16_t fft_scratch[SPECTRUM_BINS];

/*---------------------------------------------------------------------------*/
/*                        FUNCTION IMPLEMENTATION                            */
/*---------------------------------------------------------------------------*/
double spectral_frequency(int16_t samples[])
{
    uint16_t i;
    uint16_t max_index;
    int16_t max;
    uint8_t harmonic_number;
    int32_t mean = 0;
    double freq_bin = (double)SAMPLE_FREQ / (double)FRAME_SIZE;
    double delta = 0.0;

    PROFILE_MARK(1);    /* stage boundaries for bench/profile_stages.sh */

    /*
     * Remove the DC component before windowing. The AC-coupled input is biased
     * at VDD/2 and re-centred by a fixed offset, so a small residual bias can
     * remain and would otherwise leak through the window into the low bins.
     */
    for (i = 0; i < FRAME_SIZE; ++i) {
        mean += samples[i];
    }
    mean /= FRAME_SIZE;
    for (i = 0; i < FRAME_SIZE; ++i) {
        samples[i] = (int16_t)((int32_t)samples[i] - mean);
    }
    PROFILE_MARK(2);    /* DC removal done */

    /* Reduce spectral leakage before transforming. */
    window_apply_window(samples);
    PROFILE_MARK(3);    /* windowing done */

    /*
     * Real-input FFT. The real signal is packed into a half-size complex
     * sequence z[n] = x[2n] + j*x[2n+1], transformed with an N/2-point complex
     * FFT, and a split step then recovers the N/2+1 magnitude bins of the true
     * N-point spectrum. This halves both the transform work and the RAM (one
     * N/2 scratch array instead of a full FRAME_SIZE imaginary buffer) compared
     * with running a full complex FFT on a zeroed imaginary part.
     *
     * Pack: the odd samples go to the scratch (imaginary) array and the even
     * samples are compacted into the low half of the caller's buffer. The
     * even compaction reads ahead of where it writes, so it is safe in place.
     */
    for (i = 0; i < SPECTRUM_BINS; ++i) {
        fft_scratch[i] = samples[2 * i + 1];
    }
    for (i = 0; i < SPECTRUM_BINS; ++i) {
        samples[i] = samples[2 * i];
    }
    PROFILE_MARK(4);    /* real-input pack done */

    fft(samples, fft_scratch, LOG2_FRAME_SIZE - 1, 0);
    PROFILE_MARK(5);    /* FFT transform done */

    /*
     * Split the N/2-point spectrum Z (real in samples[], imaginary in
     * fft_scratch[]) into the magnitude spectrum, written back in place over
     * samples[0..SPECTRUM_BINS]. Each bin k combines Z[k] with its mirror
     * Z[N/2-k], so the pair {k, N/2-k} is computed and written together to
     * avoid clobbering a value the mirror still needs.
     */
    {
        int16_t dc_mag = real_dc_nyquist_mag(samples[0], fft_scratch[0], 1);
        int16_t nyquist_mag = real_dc_nyquist_mag(samples[0], fft_scratch[0], -1);
        uint16_t k;

        for (k = 1; k < SPECTRUM_BINS - k; ++k) {
            uint16_t m = SPECTRUM_BINS - k;
            int16_t mag_k = real_bin_mag(samples[k], fft_scratch[k], samples[m], fft_scratch[m], k);
            int16_t mag_m = real_bin_mag(samples[m], fft_scratch[m], samples[k], fft_scratch[k], m);
            samples[k] = mag_k;
            samples[m] = mag_m;
        }
        /* Self-mirrored centre bin (k == N/2-k). */
        samples[SPECTRUM_BINS / 2] = real_bin_mag(samples[SPECTRUM_BINS / 2], fft_scratch[SPECTRUM_BINS / 2],
                                                  samples[SPECTRUM_BINS / 2], fft_scratch[SPECTRUM_BINS / 2],
                                                  SPECTRUM_BINS / 2);
        samples[0] = dc_mag;
        samples[SPECTRUM_BINS] = nyquist_mag;
    }
    PROFILE_MARK(6);    /* magnitude split done */

    /* Strongest (and best frequency-resolved) bin, skipping DC (bin 0). */
    max = samples[1];
    max_index = 1;
    for (i = 2; i <= SPECTRUM_BINS; ++i) {
        if (samples[i] > max) {
            max = samples[i];
            max_index = i;
        }
    }
    PROFILE_MARK(7);    /* peak pick done */

    /*
     * Octave correction: the strongest bin is frequently a harmonic rather than
     * the fundamental, so resolve which sub-multiple of it the fundamental is.
     */
    harmonic_number = fundamental_divisor(samples, max_index, max);
    PROFILE_MARK(8);    /* octave correction done */

    /*
     * Parabolic interpolation in bin space for sub-bin frequency resolution.
     * The parabola is fitted to the log magnitudes, which is the more accurate
     * peak estimator for the (near-Gaussian) main lobe of a window; the +1
     * keeps log() finite for an empty bin. The peak is interpolated (not the
     * possibly-weak fundamental bin) and divided down, so the fundamental is
     * resolved at harmonic_number times finer absolute resolution.
     */
    if (max_index > 0 && max_index < SPECTRUM_BINS) {
        double y0 = log((double)samples[max_index - 1] + 1.0);
        double y1 = log((double)samples[max_index] + 1.0);
        double y2 = log((double)samples[max_index + 1] + 1.0);
        double denom = y0 - 2.0 * y1 + y2;
        if (denom != 0.0) {
            delta = 0.5 * (y0 - y2) / denom;
        }
    }

    PROFILE_MARK(9);    /* parabolic interpolation done */

    return ((double)max_index + delta) * freq_bin / (double)harmonic_number;
}

/**
 * @brief Magnitude of true-spectrum bin k from the half-size complex FFT.
 *        Recombines Z[k] = (ar, ai) with its mirror Z[N/2-k] = (br, bi) using
 *        the standard real-FFT split, then applies the bin's twiddle factor
 *        W = exp(-j*2*pi*k/N), and takes the magnitude with the alpha-max-plus-
 *        beta-min approximation, |z| ~= max(|re|,|im|) + min(|re|,|im|)/2,
 *        instead of an exact sqrt. The approximation is within ~12% per bin but
 *        costs only a compare, a shift and an add (no square, no sqrt); the FFT
 *        magnitude bench (bench/) confirmed it leaves pitch accuracy unchanged,
 *        since the peak pick, octave check and log-parabola use only relative
 *        bin heights. The result is a consistently-scaled magnitude.
 * @param[in] ar,ai  Real/imaginary parts of Z[k].
 * @param[in] br,bi  Real/imaginary parts of the mirror Z[N/2-k].
 * @param[in] k      Bin index (1 .. N/2-1).
 * @return Magnitude of spectral bin k.
 */
static int16_t real_bin_mag(int16_t ar, int16_t ai, int16_t br, int16_t bi, uint16_t k)
{
    int16_t xer  = (int16_t)(((int32_t)ar + br) >> 1);  /* even-sample spectrum */
    int16_t xei  = (int16_t)(((int32_t)ai - bi) >> 1);
    int16_t xor_ = (int16_t)(((int32_t)ai + bi) >> 1);  /* odd-sample spectrum */
    int16_t xoi  = (int16_t)(((int32_t)br - ar) >> 1);
    int16_t wr = SINEWAVE[k + FRAME_SIZE / 4];            /*  cos(2*pi*k/N) */
    int16_t wi = (int16_t)(-SINEWAVE[k]);               /* -sin(2*pi*k/N) */
    int16_t pr = (int16_t)(fix_mpy(wr, xor_) - fix_mpy(wi, xoi));
    int16_t pi = (int16_t)(fix_mpy(wr, xoi) + fix_mpy(wi, xor_));
    int16_t xr = (int16_t)(((int32_t)xer + pr) >> 1);
    int16_t xi = (int16_t)(((int32_t)xei + pi) >> 1);
    uint16_t ax = (uint16_t)(xr < 0 ? -xr : xr);
    uint16_t ay = (uint16_t)(xi < 0 ? -xi : xi);
    uint16_t mx = ax > ay ? ax : ay;
    uint16_t mn = ax < ay ? ax : ay;

    return (int16_t)(mx + (mn >> 1));
}

/**
 * @brief Magnitude of the purely-real DC (sign > 0) or Nyquist (sign < 0) bin,
 *        which derive from Z[0] alone. Scaled to match real_bin_mag().
 */
static int16_t real_dc_nyquist_mag(int16_t zr, int16_t zi, int8_t sign)
{
    int32_t v = (sign >= 0) ? ((int32_t)zr + zi) : ((int32_t)zr - zi);

    v >>= 2;

    return (int16_t)(v < 0 ? -v : v);
}

/**
 * @brief Decide which sub-multiple of the strongest bin is the true
 *        fundamental (HPS-style octave correction). Steps down from peak_bin to
 *        the lowest divisor m (up to FFT_MAX_SUBHARMONIC) for which every
 *        harmonic of peak_bin/m up to the peak is present in the spectrum --
 *        i.e. reaches (peak >> FFT_HARMONIC_THRESHOLD_SHIFT). Each harmonic is
 *        checked over a +-1 bin neighbourhood so an off-grid fundamental still
 *        registers. A pure tone has no supporting sub-harmonics and stays at
 *        m = 1.
 * @return The harmonic number m of peak_bin (1 = peak is the fundamental).
 */
static uint8_t fundamental_divisor(const int16_t spectrum[], uint16_t peak_bin, int16_t peak)
{
    int16_t threshold = (int16_t)(peak >> FFT_HARMONIC_THRESHOLD_SHIFT);
    uint8_t divisor = 1;
    uint8_t m;

    for (m = 2; m <= FFT_MAX_SUBHARMONIC; ++m) {
        uint16_t fundamental = (uint16_t)((peak_bin + m / 2) / m);
        uint8_t supported = 1;
        uint8_t j;

        if (fundamental < 2) {
            break;
        }

        /* Check the fundamental and the intermediate harmonics (j = m is the
         * peak itself, present by definition). */
        for (j = 1; j < m; ++j) {
            uint16_t b = (uint16_t)(((uint32_t)j * peak_bin + m / 2) / m);
            int16_t mag = spectrum[b];

            if (spectrum[b - 1] > mag) {
                mag = spectrum[b - 1];
            }
            if (b + 1 <= SPECTRUM_BINS && spectrum[b + 1] > mag) {
                mag = spectrum[b + 1];
            }
            if (mag < threshold) {
                supported = 0;
                break;
            }
        }

        if (supported) {
            divisor = m; /* lowest supported sub-harmonic wins */
        }
    }

    return divisor;
}

#endif /* PITCH_METHOD_FFT */

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
