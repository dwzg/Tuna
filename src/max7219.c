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
 * @file   max7219.c
 * @author Dennis Witzig
 * @date   2022-10-14
 * @brief  This module contains the code to interact with the MAX7219 display driver.
 */

/*---------------------------------------------------------------------------*/
/*                               INCLUDES                                    */
/*---------------------------------------------------------------------------*/
#include <stdint.h>
#include "hal.h"
#include "max7219.h"

/*---------------------------------------------------------------------------*/
/*                         DEFINITIONS AND MACROS                            */
/*---------------------------------------------------------------------------*/
#define LOW 0
#define HIGH 1
#define MAX7219_MAX_ADDRESS 0x0F

/*---------------------------------------------------------------------------*/
/*                         TYPEDEFS AND STRUCTURES                           */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                               PROTOTYPES                                  */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                            LOCAL VARIABLES                                */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                        FUNCTION IMPLEMENTATION                            */
/*---------------------------------------------------------------------------*/
/**
 * @brief Initialize MAX7219 for intended usage.
 */
void max7219_init()
{
    hal_set_din(LOW);
    hal_set_clk(LOW);
    hal_set_load(HIGH);

    max7219_reset();
    
    max7219_write(MAX7219_SCAN_LIMIT_REGISTER, 0x04); // set scan limit to digit 0 to 4
    max7219_write(MAX7219_INTENSITY_REGISTER, 0x07); // set intensity to max
    max7219_write(MAX7219_SHUTDOWN_REGISTER, 0x01); // set shutdown register to normal operation
}

/**
 * @brief Write data into a register of the MAX7219.
 * @param[in] address Address of the register to write data to.
 * @param[in] data Data to be written.
 */
void max7219_write(uint8_t address, uint8_t data)
{
    int8_t i;
    uint8_t bit;
    uint16_t command = ((uint16_t)address << 8) | data;

    hal_set_load(LOW);

    for (i = 15; i >= 0; --i) {
        bit = (uint8_t)((command >> i) & 0x0001);
        hal_set_din(bit);
        hal_set_clk(HIGH);
        hal_set_clk(LOW);
    }

    hal_set_load(HIGH);
}

/**
 * @brief Reset all registers to zero.
 */
void max7219_reset()
{
    uint8_t address;
    
    for (address = 0x00; address <= MAX7219_MAX_ADDRESS; ++address) {
        max7219_write(address, 0x00);
    }
}

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
