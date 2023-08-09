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
 * @file   max7219.h
 * @author Dennis Witzig
 * @date   2022-10-14
 * @brief  This module contains the header to interact with the MAX7219 display driver.
 */

#ifndef MAX7219_H_
#define MAX7219_H_

/*---------------------------------------------------------------------------*/
/*                               INCLUDES                                    */
/*---------------------------------------------------------------------------*/
#include <stdint.h>

/*---------------------------------------------------------------------------*/
/*                         DEFINITIONS AND MACROS                            */
/*---------------------------------------------------------------------------*/
#define MAX7219_NO_OP_REGISTER 0x00
#define MAX7219_DIGIT_0_REGISTER 0x01
#define MAX7219_DIGIT_1_REGISTER 0x02
#define MAX7219_DIGIT_2_REGISTER 0x03
#define MAX7219_DIGIT_3_REGISTER 0x04
#define MAX7219_DIGIT_4_REGISTER 0x05
#define MAX7219_DIGIT_5_REGISTER 0x06
#define MAX7219_DIGIT_6_REGISTER 0x07
#define MAX7219_DIGIT_7_REGISTER 0x08
#define MAX7219_DECODE_MODE_REGISTER 0x09
#define MAX7219_INTENSITY_REGISTER 0x0A
#define MAX7219_SCAN_LIMIT_REGISTER 0x0B
#define MAX7219_SHUTDOWN_REGISTER 0x0C
#define MAX7219_DISPLAY_TEST_REGISTER 0x0F

/*---------------------------------------------------------------------------*/
/*                         TYPEDEFS AND STRUCTURES                           */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                            GLOBAL VARIABLES                               */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                           FUNCTION PROTOTYPES                             */
/*---------------------------------------------------------------------------*/
void max7219_init();

void max7219_write(uint8_t address, uint8_t data);

void max7219_reset();

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
#endif /* MAX7219_H_ */
