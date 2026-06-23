/*
This is free and unencumbered software released into the public domain.
For more information, please refer to <http://unlicense.org/>
*/

/**
 * @file   bench_profile.c
 * @brief  Per-stage profiling harness. Identical setup to bench_dsp.c, but the
 *         stage timing comes from the PROFILE_MARK() markers inside the DSP
 *         sources (compiled with -DTUNA_PROFILE), not from the harness. The
 *         harness just loads the fixed frame, runs one pitch pass while those
 *         markers fire, then writes MARK_DONE so simprof.c knows to stop.
 *
 *         Build BENCH_FFT for the FFT path (spectral.c), else the YIN path.
 */

/*---------------------------------------------------------------------------*/
/*                               INCLUDES                                    */
/*---------------------------------------------------------------------------*/
#include <stdint.h>
#include <avr/io.h>
#include "config.h"
#include "bench_frame.h"

#ifdef BENCH_FFT
#include "spectral.h"
#define PITCH_FN spectral_frequency
#else
#include "yin.h"
#define PITCH_FN yin_frequency
#endif

/*---------------------------------------------------------------------------*/
/*                         DEFINITIONS AND MACROS                            */
/*---------------------------------------------------------------------------*/
#define MARK_DONE 0xFFu     /* simprof.c stops when it sees this on GPIOR0 */

/*---------------------------------------------------------------------------*/
/*                            GLOBAL VARIABLES                               */
/*---------------------------------------------------------------------------*/
static int16_t buf[BENCH_FRAME_SIZE];
volatile double g_result;

/*---------------------------------------------------------------------------*/
/*                                  MAIN                                     */
/*---------------------------------------------------------------------------*/
int main(void)
{
    uint16_t i;

    for (i = 0; i < BENCH_FRAME_SIZE; ++i)
        buf[i] = BENCH_FRAME[i];

    g_result = PITCH_FN(buf);   /* PROFILE_MARK()s fire during this call */

    GPIOR0 = MARK_DONE;
    for (;;)
        ;

    return 0;
}

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
