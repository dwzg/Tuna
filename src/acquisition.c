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
 * @file   acquisition.c
 * @author Dennis Witzig
 * @date   2022-10-22
 * @brief  This module contains the code for data acquisition.
 */

/*---------------------------------------------------------------------------*/
/*                               INCLUDES                                    */
/*---------------------------------------------------------------------------*/
#include <stdint.h>
#include "hal.h"
#include "acquisition.h"

/*---------------------------------------------------------------------------*/
/*                         DEFINITIONS AND MACROS                            */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                         TYPEDEFS AND STRUCTURES                           */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                               PROTOTYPES                                  */
/*---------------------------------------------------------------------------*/
static void acquisition_callback(int16_t sample);

/*---------------------------------------------------------------------------*/
/*                            LOCAL VARIABLES                                */
/*---------------------------------------------------------------------------*/
int16_t acquisition_buffer_a[FFT_SIZE];
int16_t acquisition_buffer_b[FFT_SIZE];

/* Buffer the result-ready ISR is currently writing into, and its progress. */
static int16_t *volatile fill_buffer;
static volatile uint16_t fill_index;
static volatile uint8_t fill_complete;

/*---------------------------------------------------------------------------*/
/*                        FUNCTION IMPLEMENTATION                            */
/*---------------------------------------------------------------------------*/
void acquisition_start(int16_t *buffer)
{
    fill_buffer = buffer;
    fill_index = 0;
    fill_complete = 0;

    hal_start_sample_counter(acquisition_callback);
}

void acquisition_wait(void)
{
    /*
     * The timer keeps pacing conversions until it is stopped below, so a wake
     * is always pending while the buffer fills: this loop cannot miss the
     * completion and deadlock, it only ever oversleeps by at most one sample
     * period. The CPU is asleep the rest of the time.
     */
    while (!fill_complete) {
        hal_sleep_idle();
    }

    hal_stop_sample_counter();
}

static void acquisition_callback(int16_t sample)
{
    if (fill_complete) {
        return;
    }

    fill_buffer[fill_index] = sample;
    ++fill_index;

    if (fill_index >= FFT_SIZE) {
        fill_complete = 1;
    }
}

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
