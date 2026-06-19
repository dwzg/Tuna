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
 * @file   config.h
 * @author Dennis Witzig
 * @date   2022-09-29
 * @brief  This module contains the configuration for Tuna.
 */

#ifndef CONFIG_H_
#define CONFIG_H_

/*---------------------------------------------------------------------------*/
/*                               INCLUDES                                    */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                         DEFINITIONS AND MACROS                            */
/*---------------------------------------------------------------------------*/
/**
 * @brief Full length of FFT.
 */
#define FFT_SIZE 1024

/**
 * @brief Log2 of FFT_SIZE.
 */
#define LOG2_FFT_SIZE 10

/**
 * @brief Sample frequency in Hz.
 *        Minimum is 367 Hz.
 */
#define SAMPLE_FREQ 4096UL

/**
 * @brief Window function to use.
 */
#define HAMMING

/**
 * @brief Note half step convention to use.
 */
#define SHARP

/**
 * @brief Pitch-detection method. Define exactly one of the following. The two
 *        implementations are interchangeable (both take the acquisition buffer
 *        and return a frequency in Hz) so they can be benchmarked against each
 *        other by toggling this switch.
 *          PITCH_METHOD_YIN - time-domain YIN autocorrelation (default)
 *          PITCH_METHOD_FFT - frequency-domain FFT peak picking
 */
#define PITCH_METHOD_YIN
//#define PITCH_METHOD_FFT

#if defined(PITCH_METHOD_YIN) == defined(PITCH_METHOD_FFT)
#error "config.h: define exactly one of PITCH_METHOD_YIN or PITCH_METHOD_FFT"
#endif

/**
 * @brief YIN absolute threshold. The first dip in the cumulative-mean-normalized
 *        difference function below this value is taken as the period estimate.
 *        Typical range 0.10 - 0.20; lower is stricter.
 */
#define YIN_THRESHOLD 0.15f

/*---------------------------------------------------------------------------*/
/*                         TYPEDEFS AND STRUCTURES                           */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                            GLOBAL VARIABLES                               */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                           FUNCTION PROTOTYPES                             */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/

#endif /* CONFIG_H_ */
