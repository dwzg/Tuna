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
#include "window.h"
#include "fft.h"
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
        //acquisition_fill_buffer();
        current_state = ANALYSIS;
        break;
    case ANALYSIS:
        window_apply_window(acquisition_buffer);
        fft_real(acquisition_buffer, LOG2_FFT_SIZE, 0);
        analysis_absolute(acquisition_buffer, FFT_SIZE);
        peak_freq = analysis_find_interpolated_peak_frequency(acquisition_buffer, FFT_SIZE / 2);
        current_state = DISPLAY;
        break;
    case DISPLAY:
        segment_display_num(peak_freq);
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

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
