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
 * @file   control.c
 * @author Dennis Witzig
 * @date   2022-10-21
 * @brief  This module contains the code of the state machine of Tuna.
 */

/*---------------------------------------------------------------------------*/
/*                               INCLUDES                                    */
/*---------------------------------------------------------------------------*/
#include <stdint.h>
#include "hal.h"
#include "max7219.h"
#include "display.h"
#include "segment.h"
#include "bargraph.h"
#include "acquisition.h"
#include "analysis.h"
#include "yin.h"
#include "pitch.h"
#include "smoothing.h"
#include "config.h"
#include "control.h"

/*---------------------------------------------------------------------------*/
/*                         DEFINITIONS AND MACROS                            */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                         TYPEDEFS AND STRUCTURES                           */
/*---------------------------------------------------------------------------*/
typedef enum { INIT, ACQUISITION, ANALYSIS, DISPLAY, ERROR } CONTROL_STATE;

/*---------------------------------------------------------------------------*/
/*                               PROTOTYPES                                  */
/*---------------------------------------------------------------------------*/
static void greet_message(void);
static void display_note(double frequency);
static uint8_t signal_is_present(const int16_t *buffer);

/*---------------------------------------------------------------------------*/
/*                            LOCAL VARIABLES                                */
/*---------------------------------------------------------------------------*/
static CONTROL_STATE current_state = INIT;
static double peak_freq;

/* The just-filled buffer handed back by acquisition for analysis. */
static int16_t *analysis_buffer;

/*---------------------------------------------------------------------------*/
/*                        FUNCTION IMPLEMENTATION                            */
/*---------------------------------------------------------------------------*/
/**
 * @brief State machine controller of Tuna.
 */
void control(void)
{
    switch (current_state) {
    case INIT:
        hal_init();
        max7219_init();
        greet_message();
        /* Prime the pipeline: launch the first acquisition in the background. */
        acquisition_prime();
        current_state = ACQUISITION;
        break;
    case ACQUISITION:
        /*
         * Collect the just-filled frame; acquisition immediately relaunches the
         * next fill into its other buffer, so sampling overlaps the analysis and
         * display of this one (double-buffered pipeline). The CPU sleeps inside
         * acquisition_collect() rather than spinning.
         */
        analysis_buffer = acquisition_collect();
        current_state = ANALYSIS;
        break;
    case ANALYSIS:
        /*
         * Gate on input level first: a quiet room otherwise drives the pitch
         * estimator from noise and flickers random notes. The level has to be
         * measured before analysis because both pitch methods overwrite the
         * analysis buffer in place.
         */
        if (!signal_is_present(analysis_buffer)) {
            peak_freq = 0.0;
        } else {
#if defined(PITCH_METHOD_YIN)
            peak_freq = yin_frequency(analysis_buffer);
#elif defined(PITCH_METHOD_FFT)
            peak_freq = analysis_fft_frequency(analysis_buffer);
#endif
        }
        peak_freq = smooth_frequency(peak_freq);
        current_state = DISPLAY;
        break;
    case DISPLAY:
        display_note(peak_freq);
        current_state = ACQUISITION;
        break;
    case ERROR:
    default:
        /*
         * Fault trap: unreachable in correct operation, but a corrupted state
         * variable lands here. Latch an "Er" indication once and halt the CPU
         * in low-power idle instead of re-driving the display in a tight loop.
         */
        segment_display_alpha(0, 'E');
        segment_display_alpha(1, 'R');
        for (;;) {
            hal_sleep_idle();
        }
    }
}

static void greet_message(void)
{
    bargraph_set_level(6, BARGRAPH_LEFT);
    segment_display_alpha(0, 'H');
    segment_display_alpha(1, 'I');
    display_flush();

    hal_delay_ms(1000);

    bargraph_set_level(13, BARGRAPH_LEFT);
    segment_smile();
    display_flush();

    hal_delay_ms(1000);

    bargraph_set_level(16, BARGRAPH_LEFT);
    segment_display_alpha(0, 'T');
    segment_display_alpha(1, 'U');
    display_flush();

    hal_delay_ms(500);

    bargraph_set_level(20, BARGRAPH_LEFT);
    segment_display_alpha(0, 'N');
    segment_display_alpha(1, 'A');
    display_flush();

    hal_delay_ms(500);

    display_set_digit(0, 0);
    display_set_digit(1, 0);
    bargraph_set_level(0, BARGRAPH_LEFT);
    display_flush();
}

/**
 * @brief Render the detected note on the displays: note letter (with decimal
 *        point for accidentals) on digit 0, octave on digit 1, and the cents
 *        deviation as a centred needle on the bargraph (left = flat, right =
 *        sharp). A non-positive/out-of-range frequency blanks the display.
 */
static void display_note(double frequency)
{
#ifdef SHARP
    static const char NOTE_LETTER[12] = { 'C','C','D','D','E','F','F','G','G','A','A','H' };
#else
    static const char NOTE_LETTER[12] = { 'C','D','D','E','E','F','G','G','A','A','H','H' };
#endif
    static const uint8_t NOTE_ACCIDENTAL[12] = { 0,1,0,1,0,0,1,0,1,0,1,0 };

    NOTE note = pitch_from_frequency(frequency);
    double position;

    if (!note.valid) {
        display_set_digit(0, 0);
        display_set_digit(1, 0);
        bargraph_set_binary(0);
        display_flush();
        return;
    }

    segment_display_char(0, NOTE_LETTER[note.pitch_class], NOTE_ACCIDENTAL[note.pitch_class]);
    segment_display_num_digit(1, (uint8_t)note.octave, 0);

    /* Map -50..+50 cents onto bargraph elements 0..BARGRAPH_SIZE-1. */
    position = (note.cents + 50.0) * (double)(BARGRAPH_SIZE - 1) / 100.0 + 0.5;
    if (position < 0.0) {
        position = 0.0;
    } else if (position > (double)(BARGRAPH_SIZE - 1)) {
        position = (double)(BARGRAPH_SIZE - 1);
    }

    /* Stage the needle, then push the whole frame in one flush: only the digit
     * registers that changed since last frame are actually transmitted. */
    bargraph_set_binary(1UL << (uint8_t)position);
    display_flush();
}

/**
 * @brief Report whether the just-acquired frame carries a usable signal, i.e.
 *        whether any sample deviates from the frame's DC level by at least
 *        SILENCE_THRESHOLD. Gating on the AC excursion (distance from the mean)
 *        rather than the raw sample magnitude keeps a residual DC bias on the
 *        AC-coupled input from masking a quiet signal or registering as one.
 */
static uint8_t signal_is_present(const int16_t *buffer)
{
    int32_t mean = 0;
    int16_t dc;
    uint16_t k;

    for (k = 0; k < FFT_SIZE; ++k) {
        mean += buffer[k];
    }
    dc = (int16_t)(mean / FFT_SIZE);

    for (k = 0; k < FFT_SIZE; ++k) {
        int16_t s = (int16_t)(buffer[k] - dc);
        if (s < 0) {
            s = (int16_t)-s;
        }
        if (s >= SILENCE_THRESHOLD) {
            return 1;
        }
    }

    return 0;
}

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
