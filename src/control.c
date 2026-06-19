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
static void display_note(double frequency);

/*---------------------------------------------------------------------------*/
/*                            LOCAL VARIABLES                                */
/*---------------------------------------------------------------------------*/
CONTROL_STATE current_state = INIT;
double peak_freq;
uint16_t i;

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
        current_state = ACQUISITION;
    	break;
    case ACQUISITION:
        acquisition_fill_buffer();
        current_state = ANALYSIS;
        break;
    case ANALYSIS:
#if defined(PITCH_METHOD_YIN)
        peak_freq = yin_frequency(acquisition_buffer);
#elif defined(PITCH_METHOD_FFT)
        peak_freq = analysis_fft_frequency(acquisition_buffer);
#endif
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

void greet_message()
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

    max7219_write(0x01, 0);
    max7219_write(0x02, 0);
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

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
