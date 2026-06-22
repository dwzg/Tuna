/*
This is free and unencumbered software released into the public domain.
For more information, please refer to <http://unlicense.org/>
*/

/**
 * @file   bench_dsp.c
 * @brief  AVR firmware harness that runs one pitch-detection pass over a fixed
 *         synthetic frame, bracketed by writes to GPIOR0 so the simavr-based
 *         runner (simrun.c) can read off the exact cycle count of the DSP work.
 *
 *         Build BENCH_FFT to measure the FFT path (spectral.c + fft.c +
 *         window.c); otherwise the YIN path (yin.c) is measured. The harness
 *         copies the flash-resident frame into a RAM buffer first (the pitch
 *         estimators overwrite their input in place), writes the start marker,
 *         calls the estimator, writes the end marker, then halts. Only the work
 *         between the two markers is counted, so the frame copy and any libm
 *         startup are excluded from the measurement.
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
/* GPIOR0 marker values the runner watches for (see simrun.c). */
#define MARK_START 1u
#define MARK_END   2u

#define BARRIER() __asm__ volatile("" ::: "memory")

/*---------------------------------------------------------------------------*/
/*                            GLOBAL VARIABLES                               */
/*---------------------------------------------------------------------------*/
/* Working buffer the estimator runs on (and overwrites). */
static int16_t buf[BENCH_FRAME_SIZE];

/* Sink for the result so the call cannot be optimized away. */
volatile double g_result;

/*---------------------------------------------------------------------------*/
/*                                  MAIN                                     */
/*---------------------------------------------------------------------------*/
int main(void)
{
    uint16_t i;

    for (i = 0; i < BENCH_FRAME_SIZE; ++i)
        buf[i] = BENCH_FRAME[i];

    BARRIER();
    GPIOR0 = MARK_START;        /* volatile SFR write -> start of measured region */
    BARRIER();

    g_result = PITCH_FN(buf);

    BARRIER();
    GPIOR0 = MARK_END;          /* end of measured region */
    BARRIER();

    for (;;)                    /* halt; the runner stops on MARK_END */
        ;

    return 0;
}

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
