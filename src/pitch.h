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
 * @file   pitch.h
 * @author Dennis Witzig
 * @date   2022-10-21
 * @brief  This module contains the header for pitch information.
 */

#ifndef PITCH_H_
#define PITCH_H_

/*---------------------------------------------------------------------------*/
/*                               INCLUDES                                    */
/*---------------------------------------------------------------------------*/
#include <stdint.h>
#include "config.h"

/*---------------------------------------------------------------------------*/
/*                         DEFINITIONS AND MACROS                            */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                         TYPEDEFS AND STRUCTURES                           */
/*---------------------------------------------------------------------------*/
typedef enum {
#ifdef ACCIDENTAL_SHARP
    C, C_SHARP, D, D_SHARP, E, F, F_SHARP, G, G_SHARP, A, A_SHARP, H
#else
    C, D_FLAT, D, E_FLAT, E, F, G_FLAT, G, A_FLAT, A, H_FLAT, H
#endif
} PITCH_CLASS;

/**
 * @brief Nearest note to a measured frequency, with the tuning deviation.
 */
typedef struct {
    PITCH_CLASS pitch_class; /**< Nearest pitch class (C .. H).             */
    int8_t octave;           /**< Octave index (octave 1 starts at C1).      */
    double cents;            /**< Deviation from the note, -50 .. +50 cents. */
    uint8_t valid;           /**< Non-zero if frequency mapped to a note.   */
} NOTE;

/*---------------------------------------------------------------------------*/
/*                            GLOBAL VARIABLES                               */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                           FUNCTION PROTOTYPES                             */
/*---------------------------------------------------------------------------*/
/**
 * @brief  Map a measured frequency to the nearest equal-tempered note (A4=440).
 * @param[in] frequency Measured frequency in Hz.
 * @return Nearest note and the cents deviation; .valid is 0 for a non-positive
 *         or out-of-range frequency.
 */
NOTE pitch_from_frequency(double frequency);

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
#endif /* PITCH_H_ */
