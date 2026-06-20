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
 * @file   segment.c
 * @author Dennis Witzig
 * @date   2022-10-16
 * @brief  This module contains the code for segment display control.
 */

/*---------------------------------------------------------------------------*/
/*                               INCLUDES                                    */
/*---------------------------------------------------------------------------*/
#include <stdint.h>
#include "max7219.h"
#include "segment.h"

/*---------------------------------------------------------------------------*/
/*                         DEFINITIONS AND MACROS                            */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                         TYPEDEFS AND STRUCTURES                           */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                               PROTOTYPES                                  */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                            LOCAL VARIABLES                                */
/*---------------------------------------------------------------------------*/
static const uint8_t FONT_ALPHA[26] = {
    119,  31,  78,  61,  79,  71,  94,  23,  16,  60,  87,  14, 118,
     21,  29, 103, 115,   5,  27,  15,  28,  62,  63,  55,  59, 108
};

static const uint8_t FONT_NUM[10] = {
    126,  48, 109, 121,  51,  91,  95, 114, 127, 123
};

static const uint8_t SMILE[2] = {
    44, 26
};

/*---------------------------------------------------------------------------*/
/*                        FUNCTION IMPLEMENTATION                            */
/*---------------------------------------------------------------------------*/
void segment_display_char(uint8_t digit, uint8_t alpha, uint8_t dotpoint)
{
    uint8_t glyph;

    if ('A' <= alpha && alpha <= 'Z') {
        glyph = FONT_ALPHA[alpha - 'A'];
        if (dotpoint) {
            glyph |= 0x80;
        }

        switch (digit) {
        case 0:
            max7219_write(MAX7219_DIGIT_0_REGISTER, glyph);
            break;
        case 1:
            max7219_write(MAX7219_DIGIT_1_REGISTER, glyph);
            break;
        default:
            break;
        }
    }
}

void segment_display_alpha(uint8_t digit, uint8_t alpha)
{
    segment_display_char(digit, alpha, 0);
}

void segment_display_num_digit(uint8_t digit, uint8_t value, uint8_t dotpoint)
{
    uint8_t glyph;

    if (value <= 9) {
        glyph = FONT_NUM[value];
        if (dotpoint) {
            glyph |= 0x80;
        }

        switch (digit) {
        case 0:
            max7219_write(MAX7219_DIGIT_0_REGISTER, glyph);
            break;
        case 1:
            max7219_write(MAX7219_DIGIT_1_REGISTER, glyph);
            break;
        default:
            break;
        }
    }
}

void segment_display_num(uint8_t value)
{
    if (value <= 99) {
        max7219_write(MAX7219_DIGIT_0_REGISTER, FONT_NUM[value / 10]);
        max7219_write(MAX7219_DIGIT_1_REGISTER, FONT_NUM[value % 10]);
    }
}

void segment_smile(void)
{
    max7219_write(MAX7219_DIGIT_0_REGISTER, SMILE[0]);
    max7219_write(MAX7219_DIGIT_1_REGISTER, SMILE[1]);
}

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
