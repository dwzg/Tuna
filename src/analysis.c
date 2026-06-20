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
 * @file   analysis.c
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
#include "analysis.h"

#ifdef PITCH_METHOD_FFT

#include <math.h>
#include "window.h"
#include "fft.h"

/*---------------------------------------------------------------------------*/
/*                         DEFINITIONS AND MACROS                            */
/*---------------------------------------------------------------------------*/
/**
 * @brief Number of usable spectral bins (0 .. Nyquist) of a real FFT_SIZE FFT.
 */
#define SPECTRUM_BINS (FFT_SIZE / 2)

/*---------------------------------------------------------------------------*/
/*                               PROTOTYPES                                  */
/*---------------------------------------------------------------------------*/
static uint32_t isqrt_rounded(uint32_t a_nInput);

/*---------------------------------------------------------------------------*/
/*                            LOCAL VARIABLES                                */
/*---------------------------------------------------------------------------*/
/* Imaginary part of the complex FFT (real part lives in the caller's buffer). */
static int16_t fft_imag[FFT_SIZE];

/*---------------------------------------------------------------------------*/
/*                        FUNCTION IMPLEMENTATION                            */
/*---------------------------------------------------------------------------*/
double analysis_fft_frequency(int16_t samples[])
{
    uint16_t i;
    uint16_t max_index;
    int16_t max;
    int32_t mean = 0;
    double freq_bin = (double)SAMPLE_FREQ / (double)FFT_SIZE;
    double delta = 0.0;

    /*
     * Remove the DC component before windowing. The AC-coupled input is biased
     * at VDD/2 and re-centred by a fixed offset, so a small residual bias can
     * remain and would otherwise leak through the window into the low bins.
     */
    for (i = 0; i < FFT_SIZE; ++i) {
        mean += samples[i];
    }
    mean /= FFT_SIZE;
    for (i = 0; i < FFT_SIZE; ++i) {
        samples[i] = (int16_t)((int32_t)samples[i] - mean);
    }

    /* Reduce spectral leakage before transforming. */
    window_apply_window(samples);

    /*
     * Full complex FFT of the real input (imaginary part zeroed). Unlike a
     * packed real-FFT this needs a second buffer, but it yields the true
     * spectrum so that bin k maps to frequency k * SAMPLE_FREQ / FFT_SIZE.
     */
    for (i = 0; i < FFT_SIZE; ++i) {
        fft_imag[i] = 0;
    }
    fft(samples, fft_imag, LOG2_FFT_SIZE, 0);

    /*
     * Magnitude spectrum, written in place over the (no longer needed) real
     * part. The forward FFT applies 1/FFT_SIZE fixed scaling, so magnitudes
     * stay well within int16_t range.
     */
    for (i = 0; i <= SPECTRUM_BINS; ++i) {
        uint32_t real = (uint32_t)((int32_t)samples[i] * (int32_t)samples[i]);
        uint32_t imag = (uint32_t)((int32_t)fft_imag[i] * (int32_t)fft_imag[i]);
        samples[i] = (int16_t)isqrt_rounded(real + imag);
    }

    /* Strongest bin, skipping DC (bin 0). */
    max = samples[1];
    max_index = 1;
    for (i = 2; i <= SPECTRUM_BINS; ++i) {
        if (samples[i] > max) {
            max = samples[i];
            max_index = i;
        }
    }

    /*
     * Parabolic interpolation in bin space for sub-bin frequency resolution.
     * The parabola is fitted to the log magnitudes, which is the more accurate
     * peak estimator for the (near-Gaussian) main lobe of a window; the +1
     * keeps log() finite for an empty bin.
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

    return ((double)max_index + delta) * freq_bin;
}

/**
 * @brief    Fast integer square root, with arithmetic rounding.
 * @param[in] a_nInput - unsigned integer for which to find the square root
 * @return Integer square root of the input value.
 */
static uint32_t isqrt_rounded(uint32_t a_nInput)
{
    uint32_t op  = a_nInput;
    uint32_t res = 0;
    uint32_t one = 1UL << 30; /* highest power of four <= 2^32 */

    while (one > op) {
        one >>= 2;
    }

    while (one != 0) {
        if (op >= res + one) {
            op = op - (res + one);
            res = res + 2 * one;
        }
        res >>= 1;
        one >>= 2;
    }

    /* Round to nearest integer. */
    if (op > res) {
        res++;
    }

    return res;
}

#endif /* PITCH_METHOD_FFT */

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
