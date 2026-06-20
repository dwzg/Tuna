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
 * @file   fft.h
 * @author Dennis Witzig
 * @date   2022-09-29
 * @brief  This module contains the header for the FFT implementation.
 */

#ifndef FFT_H_
#define FFT_H_

/*---------------------------------------------------------------------------*/
/*                               INCLUDES                                    */
/*---------------------------------------------------------------------------*/
#include <stdint.h>

/*---------------------------------------------------------------------------*/
/*                         DEFINITIONS AND MACROS                            */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                         TYPEDEFS AND STRUCTURES                           */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                            GLOBAL VARIABLES                               */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                           FUNCTION PROTOTYPES                             */
/*---------------------------------------------------------------------------*/
/**
 * @brief In-place forward/inverse complex fast Fourier transform.
 */
int16_t fft(int16_t fr[], int16_t fi[], int16_t m, uint8_t inverse);

/**
 * @brief Fixed-point Q15 multiply with rounding (exposed for the real-FFT
 *        split step in analysis.c).
 */
int16_t fix_mpy(int16_t a, int16_t b);

/**
 * @brief Quarter-wave-plus sine table, SINEWAVE[i] = sin(2*pi*i/FFT_SIZE) in
 *        Q15. Holds 3/4 of a period (indices 0 .. 3*FFT_SIZE/4 - 1); cosine is
 *        read as SINEWAVE[i + FFT_SIZE/4]. Shared by fft() and the real-FFT
 *        twiddle factors.
 */
extern const int16_t SINEWAVE[];

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
#endif /* FFT_H_ */
