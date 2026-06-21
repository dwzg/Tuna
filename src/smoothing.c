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
 * @file   smoothing.c
 * @author Dennis Witzig
 * @date   2026-06-20
 * @brief  This module stabilises the per-frame fundamental-frequency estimate
 *         (exponential moving average plus octave-jump rejection) before it is
 *         displayed. It is plain floating-point C with no AVR dependencies, so
 *         it is exercised by the host regression tests.
 */

/*---------------------------------------------------------------------------*/
/*                               INCLUDES                                    */
/*---------------------------------------------------------------------------*/
#include <stdint.h>
#include "config.h"
#include "smoothing.h"

/*---------------------------------------------------------------------------*/
/*                         DEFINITIONS AND MACROS                            */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                         TYPEDEFS AND STRUCTURES                           */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                               PROTOTYPES                                  */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                            LOCAL VARIABLES                                */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                        FUNCTION IMPLEMENTATION                            */
/*---------------------------------------------------------------------------*/
double smooth_frequency(double raw)
{
    static double smoothed = 0.0;
    static uint8_t have = 0;
    static uint8_t octave_votes = 0;

    double corrected;
    double ratio;

    if (raw <= 0.0) {
        have = 0;
        smoothed = 0.0;
        octave_votes = 0;
        return 0.0;
    }

    if (!have) {
        smoothed = raw;
        have = 1;
        octave_votes = 0;
        return smoothed;
    }

    ratio = raw / smoothed;

    if (ratio > 1.8 && ratio < 2.2) {
        corrected = raw * 0.5;
    } else if (ratio > 0.45 && ratio < 0.55) {
        corrected = raw * 2.0;
    } else {
        corrected = 0.0; /* not an octave artifact */
    }

    if (corrected != 0.0) {
        /* Hold the previous estimate unless the new octave keeps recurring. */
        if (++octave_votes < OCTAVE_JUMP_FRAMES) {
            return smoothed;
        }
        smoothed = raw;
        octave_votes = 0;
        return smoothed;
    }

    octave_votes = 0;
    corrected = raw;
    ratio = corrected / smoothed;

    if (ratio < 0.97 || ratio > 1.03) {
        /* More than ~half a semitone away: a real note change, snap to it. */
        smoothed = corrected;
        return smoothed;
    }

    /* Same note held: smooth to steady the cents readout. */
    smoothed = SMOOTHING_ALPHA * corrected + (1.0 - SMOOTHING_ALPHA) * smoothed;
    return smoothed;
}

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
