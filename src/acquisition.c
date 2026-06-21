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
#include "config.h"
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
/*
 * The ADC fills this ring continuously in the background; it holds the most
 * recent FRAME_SIZE samples at all times. Successive analysis windows are taken
 * from it HOP_SIZE samples apart, so they overlap and a reading is produced
 * every HOP_SIZE samples instead of once per full frame.
 */
static int16_t acquisition_ring[FRAME_SIZE];

/*
 * A private copy of the current window handed back for analysis. The ring stays
 * intact so the next (overlapping) window can reuse the retained history, while
 * the pitch estimator is free to overwrite this copy in place. The second
 * FRAME_SIZE buffer is the same SRAM cost as the old double-buffered scheme.
 */
static int16_t analysis_window[FRAME_SIZE];

/*
 * Producer (ISR) state. head is the next ring slot to write and therefore also
 * points at the oldest of the FRAME_SIZE samples currently held. fill_level
 * gates the first window until the ring has filled once; samples_since_hop
 * counts towards the next hop boundary; hop_ready hands a completed hop to the
 * consumer.
 */
static volatile uint16_t head;
static volatile uint16_t fill_level;
static volatile uint16_t samples_since_hop;
static volatile uint8_t hop_ready;

/*---------------------------------------------------------------------------*/
/*                        FUNCTION IMPLEMENTATION                            */
/*---------------------------------------------------------------------------*/
void acquisition_prime(void)
{
    head = 0;
    fill_level = 0;
    samples_since_hop = 0;
    hop_ready = 0;

    /*
     * Start the ADC filling the ring and never stop it: unlike the old
     * double-buffered scheme the sample counter runs continuously, so sampling
     * never pauses between frames and consecutive windows are seamless.
     */
    hal_start_sample_counter(acquisition_callback);
}

int16_t *acquisition_collect(void)
{
    uint16_t start;
    uint16_t i;

    /*
     * Sleep until the producer has laid down another HOP_SIZE samples (or, if
     * analysis ran longer than a hop, until the next already-elapsed hop).
     * hop_ready is only raised once the ring holds a full frame, so the first
     * window returned is always fully valid. A wake is always pending while the
     * timer keeps pacing conversions, so this cannot miss a hop and deadlock.
     */
    while (!hop_ready) {
        hal_sleep_idle();
    }

    /* Latch the window origin and consume the hop flag atomically: head is a
     * 16-bit value updated by the ISR and cannot be read in one instruction. */
    hal_disable_interrupts();
    start = head;
    hop_ready = 0;
    hal_enable_interrupts();

    /*
     * Copy the FRAME_SIZE most-recent samples, oldest first, into the private
     * window: start is the oldest sample and the newest lands last. The copy
     * walks forward in the same direction the producer advances, but at hundreds
     * of cycles per sample against the one-conversion-per-244us rate it stays
     * far ahead of the producer for the whole frame, so no slot is overwritten
     * mid-copy and the snapshot is consistent without holding off interrupts.
     */
    for (i = 0; i < FRAME_SIZE; ++i) {
        analysis_window[i] = acquisition_ring[start];
        start = (start + 1 == FRAME_SIZE) ? 0 : start + 1;
    }

    return analysis_window;
}

static void acquisition_callback(int16_t sample)
{
    acquisition_ring[head] = sample;
    head = (head + 1 == FRAME_SIZE) ? 0 : head + 1;

    if (fill_level < FRAME_SIZE) {
        ++fill_level;
    }

    if (++samples_since_hop >= HOP_SIZE) {
        samples_since_hop = 0;
        if (fill_level >= FRAME_SIZE) {
            hop_ready = 1;
        }
    }
}

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
