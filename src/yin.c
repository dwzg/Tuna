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
#include "profile.h"

#ifdef PITCH_METHOD_YIN

/*---------------------------------------------------------------------------*/
/*                         DEFINITIONS AND MACROS                            */
/*---------------------------------------------------------------------------*/
/**
 * @brief Integration window length: number of sample pairs summed per lag.
 *        Overridable from the build (e.g. the accuracy/cost sweep) but defaults
 *        to half the frame; the dominant loop costs O(YIN_W * YIN_TAU_MAX).
 */
#ifndef YIN_W
#define YIN_W (FRAME_SIZE / 2)
#endif

/**
 * @brief Maximum lag. Sets the lowest detectable frequency = SAMPLE_FREQ / YIN_TAU_MAX.
 *        With SAMPLE_FREQ=4096 this is 25.6 Hz, still below the lowest bass
 *        string (a 5-string low B is ~31 Hz), so the usable range is unaffected.
 *        The difference-function cost is O(YIN_W * YIN_TAU_MAX) in the worst
 *        case and dominates the whole estimator, so this lag is the main speed
 *        knob (the fused threshold search below exits the loop early once a
 *        note's period is found, so the full cost is only paid on frames with
 *        no detectable dip). 160 was picked with
 *        bench/sweep.sh as the smallest lag with no measured accuracy change from
 *        the original 256 (identical gross-miss count and ~2 cent median error),
 *        cutting the difference loop by 38% (from ~150% to ~93% of the frame
 *        budget). The window only needs YIN_W + YIN_TAU_MAX <= FRAME_SIZE samples.
 *        Overridable from the build (e.g. the accuracy/cost sweep).
 */
#ifndef YIN_TAU_MAX
#define YIN_TAU_MAX 160
#endif

/**
 * @brief Smallest lag considered, i.e. the highest detectable frequency
 *        (SAMPLE_FREQ / YIN_TAU_MIN). Skips the trivial lags 0/1.
 */
#define YIN_TAU_MIN 2

/*
 * The difference function reads samples[j + tau] for j < YIN_W and
 * tau < YIN_TAU_MAX, so the window and maximum lag together must fit in one
 * frame. YIN_W and YIN_TAU_MAX are build-overridable (the accuracy/cost
 * sweep), so guard the combination instead of trusting it.
 */
#if YIN_W + YIN_TAU_MAX > FRAME_SIZE
#error "yin.c: YIN_W + YIN_TAU_MAX must not exceed FRAME_SIZE"
#endif

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
    uint8_t dipping = 0;
    uint64_t running_sum = 0;
    double better_tau;

    cmnd[0] = 1.0f;

    PROFILE_MARK(1);    /* stage boundaries for bench/profile_stages.sh */

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
     *
     * The absolute-threshold search is fused into the same pass so the loop
     * can exit early: cmnd[tau] depends only on lags <= tau, so once the first
     * dip below YIN_THRESHOLD has passed its local minimum, no later lag can
     * change the estimate and the remaining difference-function work is
     * skipped. The period of a detected note is tau_est ~ SAMPLE_FREQ / f0
     * lags, so the higher the note the earlier the exit -- an A4 stops after
     * ~12 of the YIN_TAU_MAX lags. The worst case (no dip: silence gated
     * upstream, or an inharmonic frame) still runs the full range.
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

        if (tau < YIN_TAU_MIN) {
            continue;
        }
        if (dipping) {
            if (cmnd[tau] >= cmnd[tau - 1]) {
                /* The dip bottomed out at the previous lag; cmnd[tau] is the
                 * rising neighbour the parabolic interpolation needs. */
                tau_est = (uint16_t)(tau - 1);
                break;
            }
        } else if (cmnd[tau] < YIN_THRESHOLD) {
            dipping = 1;
        }
    }

    /* Still descending at the last lag: the minimum is the last lag itself. */
    if (dipping && tau_est == 0) {
        tau_est = YIN_TAU_MAX - 1;
    }

    PROFILE_MARK(2);    /* difference + CMND + fused threshold search done */

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

    PROFILE_MARK(3);    /* fallback minimum search done */

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

    PROFILE_MARK(4);    /* parabolic interpolation done */

    if (better_tau <= 0.0) {
        return 0.0;
    }

    return (double)SAMPLE_FREQ / better_tau;
}

#endif /* PITCH_METHOD_YIN */

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
