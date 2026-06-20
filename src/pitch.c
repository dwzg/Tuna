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
 * @file   pitch.c
 * @author Dennis Witzig
 * @date   2022-10-21
 * @brief  This module contains the code and data for pitch information.
 */

/*---------------------------------------------------------------------------*/
/*                               INCLUDES                                    */
/*---------------------------------------------------------------------------*/
#include <stdint.h>
#include <math.h>
#include "pitch.h"

/*---------------------------------------------------------------------------*/
/*                         DEFINITIONS AND MACROS                            */
/*---------------------------------------------------------------------------*/
#define NUM_OCTAVES 9
#define NUM_PITCH_CLASSES 12

/* MIDI note 69 is A4 = 440 Hz; one octave per 12 semitones. */
#define A4_MIDI 69
#define A4_FREQ 440.0
#define LN2 0.69314718055994531

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
NOTE pitch_from_frequency(double frequency)
{
    NOTE note = { C, 0, 0.0, 0 };
    double midi;
    int16_t nearest;
    int8_t octave;

    if (frequency <= 0.0) {
        return note;
    }

    /* Fractional MIDI note number, then the nearest integer semitone. */
    midi = (double)A4_MIDI + 12.0 * (log(frequency / A4_FREQ) / LN2);
    nearest = (int16_t)(midi + 0.5);

    /* Octave numbering follows scientific pitch notation (octave 1 starts at C1). */
    octave = (int8_t)(nearest / NUM_PITCH_CLASSES - 1);
    if (octave < 0 || octave >= NUM_OCTAVES) {
        return note;
    }

    note.pitch_class = (PITCH_CLASS)(nearest % NUM_PITCH_CLASSES);
    note.octave = octave;
    note.cents = (midi - (double)nearest) * 100.0;
    note.valid = 1;

    return note;
}

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
