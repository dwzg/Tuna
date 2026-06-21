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
 * @file   display.h
 * @author Dennis Witzig
 * @date   2026-06-21
 * @brief  This module is a small shadow framebuffer over the MAX7219 digit
 *         registers. The segment and bargraph renderers stage digit values
 *         here; a single display_flush() then pushes only the digits that
 *         actually changed, coalescing a frame's updates into one atomic burst
 *         and skipping registers whose contents are unchanged.
 */

#ifndef DISPLAY_H_
#define DISPLAY_H_

/*---------------------------------------------------------------------------*/
/*                               INCLUDES                                    */
/*---------------------------------------------------------------------------*/
#include <stdint.h>

/*---------------------------------------------------------------------------*/
/*                         DEFINITIONS AND MACROS                            */
/*---------------------------------------------------------------------------*/
/**
 * @brief Number of MAX7219 digit registers the framebuffer covers, mapped to
 *        digit indices 0..DISPLAY_DIGITS-1 (MAX7219_DIGIT_0..DIGIT_4): digits
 *        0/1 are the 7-segment characters, digits 2/3/4 drive the bargraph.
 */
#define DISPLAY_DIGITS 5

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
 * @brief Stage a digit value into the shadow framebuffer. Nothing is sent to
 *        the MAX7219 until display_flush() is called.
 * @param[in] digit Digit index 0..DISPLAY_DIGITS-1 (out-of-range is ignored).
 * @param[in] value Raw segment byte for that digit register.
 */
void display_set_digit(uint8_t digit, uint8_t value);

/**
 * @brief Push every staged digit whose value differs from what the MAX7219 was
 *        last given, then mark them as shipped. Digits that did not change cost
 *        no bus traffic.
 */
void display_flush(void);

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
#endif /* DISPLAY_H_ */
