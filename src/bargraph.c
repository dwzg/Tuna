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
 * @file   bargraph.c
 * @author Dennis Witzig
 * @date   2022-10-15
 * @brief  This module contains the code to interact with a bar graph display
 *         connected to a MAX7219 display driver.
 */

/*---------------------------------------------------------------------------*/
/*                               INCLUDES                                    */
/*---------------------------------------------------------------------------*/
#include <stdint.h>
#include "max7219.h"
#include "bargraph.h"

/*---------------------------------------------------------------------------*/
/*                         DEFINITIONS AND MACROS                            */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                         TYPEDEFS AND STRUCTURES                           */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                               PROTOTYPES                                  */
/*---------------------------------------------------------------------------*/
static void bargraph_send_data(void);

/*---------------------------------------------------------------------------*/
/*                            LOCAL VARIABLES                                */
/*---------------------------------------------------------------------------*/
static uint32_t bargraph_state = 0;

/*---------------------------------------------------------------------------*/
/*                        FUNCTION IMPLEMENTATION                            */
/*---------------------------------------------------------------------------*/
/**
 * @brief Send current bar graph state to the display driver.
 */
static void bargraph_send_data(void)
{
    uint8_t data_dig_2, data_dig_3, data_dig_4;

    data_dig_2 = (uint8_t)((bargraph_state & 0x000FE000) >> 13) | (uint8_t)((bargraph_state & 0x00001000) >> 5);
    data_dig_3 = (uint8_t)((bargraph_state & 0x00000FE0) >> 5) | (uint8_t)((bargraph_state & 0x00000010) << 3);
    data_dig_4 = (uint8_t)(bargraph_state & 0x0000000F) << 3;

    max7219_write(MAX7219_DIGIT_2_REGISTER, data_dig_2);
    max7219_write(MAX7219_DIGIT_3_REGISTER, data_dig_3);
    max7219_write(MAX7219_DIGIT_4_REGISTER, data_dig_4);
}

/**
 * @brief Sets a given element of the bar graph to the given value.
 * @param[in] element Bar graph element to set, going left to right.
 * @param[in] value Value to set element to.
 */
void bargraph_set_element(uint8_t element, uint8_t value)
{
    if (element < BARGRAPH_SIZE) {
        if (value == BARGRAPH_OFF) {
            bargraph_state &= ~(1UL << element);
        } else {
            bargraph_state |= (1UL << element);
        }
    }

    bargraph_send_data();
}

/**
 * @brief Sets the bar graph to a given level bar starting from origin.
 * @param[in] level Level bar to display.
 * @param[in] origin Origin of the level bar.
 */
void bargraph_set_level(uint8_t level, uint8_t origin)
{
    uint32_t bit_mask;

    if (level <= BARGRAPH_SIZE) {
        bit_mask = (1UL << level) - 1UL;
        if (origin == BARGRAPH_LEFT) {
            bargraph_state = bit_mask;
        } else {
            bargraph_state = bit_mask << (BARGRAPH_SIZE - level);
        }
    }

    bargraph_send_data();
}

void bargraph_set_binary(int16_t value)
{
    bargraph_state = (uint32_t)value;

    bargraph_send_data();
}

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
