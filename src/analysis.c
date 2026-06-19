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
 * @file   analysis.c
 * @author Dennis Witzig
 * @date   2022-10-21
 * @brief  This module contains the code to analyze the audio spectrum and
 *         determine the main frequency or note of the signal.
 */

/*---------------------------------------------------------------------------*/
/*                               INCLUDES                                    */
/*---------------------------------------------------------------------------*/
#include <stdint.h>
#include "config.h"
#include "pitch.h"
#include "bargraph.h"
#include "analysis.h"

/*---------------------------------------------------------------------------*/
/*                         DEFINITIONS AND MACROS                            */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                         TYPEDEFS AND STRUCTURES                           */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                               PROTOTYPES                                  */
/*---------------------------------------------------------------------------*/
uint32_t isqrt_rounded(uint32_t a_nInput);
void calculate_parabola_coefficients(point p1, point p2, point p3, double *a, double *b, double *c);
double calculate_parabola_maximum(double a, double b);

/*---------------------------------------------------------------------------*/
/*                            LOCAL VARIABLES                                */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                        FUNCTION IMPLEMENTATION                            */
/*---------------------------------------------------------------------------*/
void analysis_absolute(int16_t spectrum[], uint16_t size)
{
    uint16_t i;
    uint16_t half_size = size / 2;
    uint32_t real, imag;

    for (i = 0; i < half_size; ++i) {
        real = (uint32_t)((int32_t)spectrum[i] * (int32_t)spectrum[i]);
        imag = (uint32_t)((int32_t)spectrum[half_size + i] * (int32_t)spectrum[half_size + i]);

        // PROBLEM WITH DATA TYPE: sqrt(32767^2 + 32767^2) = 46340
        spectrum[i] = (int16_t)(isqrt_rounded(real + imag) / 2);
    }
}

uint16_t analysis_find_peak_frequency(int16_t spectrum[], uint16_t size, uint16_t *index)
{
    uint16_t i;
    uint16_t half_size = size / 2;
    uint16_t freq_bin = SAMPLE_FREQ / size;
    uint16_t max = 0;
    uint16_t max_index = 0;

    for (i = 0; i < half_size; ++i) {
        if (spectrum[i] > max) {
            max = spectrum[i];
            max_index = i;
        }
    }

    *index = max_index;

    return max_index * freq_bin;
}

double analysis_find_interpolated_peak_frequency(int16_t spectrum[], uint16_t size)
{
    double a, b, c;
    double peak = 0.0;
    point p1, p2, p3;
    uint16_t index;
    double freq_bin = (double)SAMPLE_FREQ / (double)size;

    analysis_find_peak_frequency(spectrum, size, &index);

    if (0 < index && index < size - 2) {
        p1.x = (double)((index - 1) * freq_bin);
        p1.y = (double)spectrum[index - 1];

        p2.x = (double)(index * freq_bin);
        p2.y = (double)spectrum[index];

        p3.x = (double)((index + 1) * freq_bin);
        p3.y = (double)spectrum[index + 1];

        calculate_parabola_coefficients(p1, p2, p3, &a, &b, &c);
        bargraph_set_binary(c);
        peak = calculate_parabola_maximum(a, b);
    }

    return peak;
}

/**
 * @brief    Fast Square root algorithm, with rounding
 *
 * This does arithmetic rounding of the result. That is, if the real answer
 * would have a fractional part of 0.5 or greater, the result is rounded up to
 * the next integer.
 *      - SquareRootRounded(2) --> 1
 *      - SquareRootRounded(3) --> 2
 *      - SquareRootRounded(4) --> 2
 *      - SquareRootRounded(6) --> 2
 *      - SquareRootRounded(7) --> 3
 *      - SquareRootRounded(8) --> 3
 *      - SquareRootRounded(9) --> 3
 *
 * @param[in] a_nInput - unsigned integer for which to find the square root
 * @return Integer square root of the input value.
 */
uint32_t isqrt_rounded(uint32_t a_nInput)
{
    uint32_t op  = a_nInput;
    uint32_t res = 0;
    uint32_t one = 1UL << 30; // The second-to-top bit is set: use 1u << 14 for uint16_t type; use 1uL<<30 for uint32_t type

    // "one" starts at the highest power of four <= than the argument.
    while (one > op) {
        one >>= 2;
    }

    while (one != 0) {
        if (op >= res + one) {
            op = op - (res + one);
            res = res +  2 * one;
        }
        res >>= 1;
        one >>= 2;
    }

    /* Do arithmetic rounding to nearest integer */
    if (op > res) {
        res++;
    }

    return res;
}

void calculate_parabola_coefficients(point p1, point p2, point p3, double *a, double *b, double *c)
{
    *a = ((p1.y - p2.y) - (p2.y - p3.y) * (p1.x - p2.x) / (p2.x - p3.x)) / ((p1.x * p1.x - p2.x * p2.x) - (p2.x * p2.x - p3.x * p3.x) * (p1.x - p2.x) / (p2.x - p3.x));
    *b = ((p1.y - p2.y) - *a * (p1.x * p1.x - p2.x * p2.x)) / (p1.x - p2.x);
    *c = p1.y - *a * (p1.x * p1.x) - *b * p1.x;
}

double calculate_parabola_maximum(double a, double b)
{
    return -0.5 * (a / b);
}

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
