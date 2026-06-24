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

/*---------------------------------------------------------------------------*/
/*                           FUNCTION PROTOTYPES                             */
/*---------------------------------------------------------------------------*/
/**
 * @brief Start the acquisition pipeline by launching the continuous background
 *        fill of the internal sample ring. Call once before the first
 *        acquisition_collect().
 */
void acquisition_prime(void);

/**
 * @brief Collect the most recent FRAME_SIZE samples as the next analysis window
 *        and hand them back for analysis. The ADC fills a rolling history ring
 *        in the background, so successive windows are taken HOP_SIZE samples
 *        apart and overlap by FRAME_SIZE - HOP_SIZE: a reading is produced every
 *        HOP_SIZE samples rather than once per full frame. Blocks (sleeping the
 *        CPU) until the next hop's worth of fresh samples has arrived.
 * @return Pointer to a FRAME_SIZE window owned by the caller until the next
 *         acquisition_collect(); it is a private copy and may be overwritten in
 *         place during analysis without disturbing the retained ring history.
 */
int16_t *acquisition_collect(void);

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
#endif /* ACQUISITION_H_ */
