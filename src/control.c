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
#include "segment.h"
#include "bargraph.h"
#include "acquisition.h"
#include "analysis.h"
#include "yin.h"
#include "pitch.h"
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
static double smooth_frequency(double raw);

/*---------------------------------------------------------------------------*/
/*                            LOCAL VARIABLES                                */
/*---------------------------------------------------------------------------*/
static CONTROL_STATE current_state = INIT;
static double peak_freq;

/* The just-filled buffer being analysed, and the one the ADC is filling next. */
static int16_t *analysis_buffer;
static int16_t *filling_buffer;

/*---------------------------------------------------------------------------*/
/*                        FUNCTION IMPLEMENTATION                            */
/*---------------------------------------------------------------------------*/
/**
 * @brief State machine controller of Tuna.
 */
void control()
{
    switch (current_state) {
    case INIT:
        hal_init();
        max7219_init();
        greet_message();
        /* Prime the pipeline: launch the first acquisition in the background. */
        filling_buffer = acquisition_buffer_a;
        acquisition_start(filling_buffer);
        current_state = ACQUISITION;
        break;
    case ACQUISITION:
        /*
         * Collect the buffer the ADC has been filling, then immediately launch
         * the next acquisition into the other buffer so sampling overlaps the
         * analysis and display of this one (double-buffered pipeline). The CPU
         * sleeps inside acquisition_wait() rather than spinning.
         */
        acquisition_wait();
        analysis_buffer = filling_buffer;
        filling_buffer = (filling_buffer == acquisition_buffer_a)
                       ? acquisition_buffer_b
                       : acquisition_buffer_a;
        acquisition_start(filling_buffer);
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
        segment_display_alpha(0, 'E');
        segment_display_alpha(1, 'R');
        current_state = ERROR;
        break;
    default:
        current_state = ERROR;
        break;
    }
}

static void greet_message(void)
{
    bargraph_set_level(6, BARGRAPH_LEFT);
    segment_display_alpha(0, 'H');
    segment_display_alpha(1, 'I');

    hal_delay_ms(1000);

    bargraph_set_level(13, BARGRAPH_LEFT);
    segment_smile();

    hal_delay_ms(1000);

    bargraph_set_level(16, BARGRAPH_LEFT);
    segment_display_alpha(0, 'T');
    segment_display_alpha(1, 'U');

    hal_delay_ms(500);

    bargraph_set_level(20, BARGRAPH_LEFT);
    segment_display_alpha(0, 'N');
    segment_display_alpha(1, 'A');

    hal_delay_ms(500);

    max7219_write(MAX7219_DIGIT_0_REGISTER, 0);
    max7219_write(MAX7219_DIGIT_1_REGISTER, 0);
    bargraph_set_level(0, BARGRAPH_LEFT);
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
        max7219_write(MAX7219_DIGIT_0_REGISTER, 0);
        max7219_write(MAX7219_DIGIT_1_REGISTER, 0);
        bargraph_set_binary(0);
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

    bargraph_set_binary(0);
    bargraph_set_element((uint8_t)position, BARGRAPH_ON);
}

/**
 * @brief Report whether the just-acquired frame carries a usable signal, i.e.
 *        whether any sample reaches SILENCE_THRESHOLD in magnitude. Returns on
 *        the first loud sample, so a present signal costs almost nothing.
 */
static uint8_t signal_is_present(const int16_t *buffer)
{
    uint16_t k;

    for (k = 0; k < FFT_SIZE; ++k) {
        int16_t s = buffer[k];
        if (s < 0) {
            s = (int16_t)-s;
        }
        if (s >= SILENCE_THRESHOLD) {
            return 1;
        }
    }

    return 0;
}

/**
 * @brief Stabilise the per-frame frequency estimate before it is displayed.
 *        A non-positive input (silence) blanks the reading and resets the
 *        filter. Otherwise the estimate is smoothed with an exponential moving
 *        average while a note is held, transient half/double-pitch errors are
 *        rejected (but accepted once an octave change persists), and a genuine
 *        change of more than ~half a semitone snaps through immediately.
 */
static double smooth_frequency(double raw)
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
        if (++octave_votes < OCTAVE_GIVE_IN) {
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
