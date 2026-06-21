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
 * @file   yin.c
 * @author Dennis Witzig
 * @date   2026-06-19
 * @brief  Time-domain fundamental-frequency estimation using the YIN algorithm
 *         (de Cheveigne & Kawahara, 2002). Steps 1-5 of the paper are applied:
 *         difference function, cumulative mean normalization, absolute
 *         threshold, and parabolic interpolation of the chosen lag.
 */

/*---------------------------------------------------------------------------*/
/*                               INCLUDES                                    */
/*---------------------------------------------------------------------------*/
#include <stdint.h>
#include "config.h"
#include "yin.h"

#ifdef PITCH_METHOD_YIN

/*---------------------------------------------------------------------------*/
/*                         DEFINITIONS AND MACROS                            */
/*---------------------------------------------------------------------------*/
/**
 * @brief Integration window length: number of sample pairs summed per lag.
 */
#define YIN_W (FRAME_SIZE / 2)

/**
 * @brief Maximum lag. Sets the lowest detectable frequency = SAMPLE_FREQ / YIN_TAU_MAX.
 *        With SAMPLE_FREQ=4096 this is 16 Hz, still well below the lowest bass
 *        string (a 5-string low B is ~31 Hz), so the extra range up to 8 Hz that
 *        a larger lag would buy is unused. The difference-function cost is
 *        O(YIN_W * YIN_TAU_MAX), so halving the lag halves the dominant loop.
 *        The window only needs YIN_W + YIN_TAU_MAX <= FRAME_SIZE samples.
 */
#define YIN_TAU_MAX 256

/**
 * @brief Smallest lag considered, i.e. the highest detectable frequency
 *        (SAMPLE_FREQ / YIN_TAU_MIN). Skips the trivial lags 0/1.
 */
#define YIN_TAU_MIN 2

/*---------------------------------------------------------------------------*/
/*                            LOCAL VARIABLES                                */
/*---------------------------------------------------------------------------*/
/* Cumulative-mean-normalized difference function, indexed by lag. */
static float cmnd[YIN_TAU_MAX];

/*---------------------------------------------------------------------------*/
/*                        FUNCTION IMPLEMENTATION                            */
/*---------------------------------------------------------------------------*/
double yin_frequency(int16_t samples[])
{
    uint16_t tau, j;
    uint16_t tau_est = 0;
    uint64_t running_sum = 0;
    double better_tau;

    cmnd[0] = 1.0f;

    /*
     * Difference function d(tau) plus cumulative mean normalization in one
     * pass. The squared differences are summed in a 32-bit accumulator: each
     * per-sample difference is pre-scaled by one bit (>>1) so that YIN_W terms,
     * each at most (4095/2)^2, cannot overflow uint32. That lets the inner loop
     * use an inline 16x16->32 widening multiply and a 32-bit add instead of the
     * __mulsi3 (32-bit multiply) and __adddi3 (64-bit add) libgcc helper calls
     * that otherwise run on every one of the YIN_W * YIN_TAU_MAX iterations and
     * dominate the cost on an 8-bit AVR. The 1-bit scale cancels in the cmnd
     * ratio, so it does not affect the result. running_sum stays a 64-bit
     * accumulator (its total reaches ~10^11, beyond uint32) but is updated only
     * once per tau, and the single floating-point divide is deferred to the
     * normalization step.
     */
    for (tau = 1; tau < YIN_TAU_MAX; ++tau) {
        uint32_t acc = 0;
        for (j = 0; j < YIN_W; ++j) {
            int16_t d = (int16_t)((samples[j] - samples[j + tau]) >> 1);
            acc += (uint32_t)((int32_t)d * d);
        }
        running_sum += acc;
        cmnd[tau] = (running_sum > 0)
                  ? (float)((double)acc * (double)tau / (double)running_sum)
                  : 1.0f;
    }

    /* Absolute threshold: first dip below YIN_THRESHOLD, descended to its local minimum. */
    for (tau = YIN_TAU_MIN; tau < YIN_TAU_MAX - 1; ++tau) {
        if (cmnd[tau] < YIN_THRESHOLD) {
            while (tau + 1 < YIN_TAU_MAX && cmnd[tau + 1] < cmnd[tau]) {
                ++tau;
            }
            tau_est = tau;
            break;
        }
    }

    /* Nothing crossed the threshold: fall back to the global minimum. */
    if (tau_est == 0) {
        float min_val = cmnd[YIN_TAU_MIN];
        tau_est = YIN_TAU_MIN;
        for (tau = YIN_TAU_MIN + 1; tau < YIN_TAU_MAX - 1; ++tau) {
            if (cmnd[tau] < min_val) {
                min_val = cmnd[tau];
                tau_est = tau;
            }
        }
    }

    /* Parabolic interpolation of the lag around tau_est for sub-sample accuracy. */
    better_tau = (double)tau_est;
    if (tau_est > 0 && tau_est < YIN_TAU_MAX - 1) {
        float s0 = cmnd[tau_est - 1];
        float s1 = cmnd[tau_est];
        float s2 = cmnd[tau_est + 1];
        double denom = (double)(s0 + s2 - 2.0f * s1);
        if (denom != 0.0) {
            better_tau = (double)tau_est + (double)(s0 - s2) / (2.0 * denom);
        }
    }

    if (better_tau <= 0.0) {
        return 0.0;
    }

    return (double)SAMPLE_FREQ / better_tau;
}

#endif /* PITCH_METHOD_YIN */

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
