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
 * @file   acquisition.h
 * @author Dennis Witzig
 * @date   2022-10-22
 * @brief  This module contains the header for data acquisition.
 */

#ifndef ACQUISITION_H_
#define ACQUISITION_H_

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

/*---------------------------------------------------------------------------*/
/*                            GLOBAL VARIABLES                               */
/*---------------------------------------------------------------------------*/
/*
 * Two acquisition buffers so sampling can run ahead of analysis: while one
 * buffer is being analysed (and overwritten in place by the pitch estimator)
 * the ADC fills the other in the background. The SRAM for the second buffer is
 * the cost of pipelining acquisition and analysis.
 */
extern int16_t acquisition_buffer_a[FFT_SIZE];
extern int16_t acquisition_buffer_b[FFT_SIZE];

/*---------------------------------------------------------------------------*/
/*                           FUNCTION PROTOTYPES                             */
/*---------------------------------------------------------------------------*/
/**
 * @brief Begin filling the given buffer from the ADC in the background. Returns
 *        immediately; the buffer must not be touched until acquisition_wait().
 */
void acquisition_start(int16_t *buffer);

/**
 * @brief Block (sleeping between samples) until the in-flight acquisition
 *        started by acquisition_start() has filled its buffer.
 */
void acquisition_wait(void);

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
#endif /* ACQUISITION_H_ */
