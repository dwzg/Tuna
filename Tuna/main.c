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
 * @file   main.c
 * @author Dennis Witzig
 * @date   2022-09-29
 * @brief  This module contains the main code for Tuna.
 */

/*---------------------------------------------------------------------------*/
/*                               INCLUDES                                    */
/*---------------------------------------------------------------------------*/
#include <stdint.h>
#include "config.h"
#include "hal.h"
#include "max7219.h"
#include "segment.h"
#include "bargraph.h"
#include "acquisition.h"
#include "window.h"
#include "fft.h"
#include "control.h"

/*---------------------------------------------------------------------------*/
/*                         DEFINITIONS AND MACROS                            */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                         TYPEDEFS AND STRUCTURES                           */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                               PROTOTYPES                                  */
/*---------------------------------------------------------------------------*/
void greet_message();

/*---------------------------------------------------------------------------*/
/*                            LOCAL VARIABLES                                */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                        FUNCTION IMPLEMENTATION                            */
/*---------------------------------------------------------------------------*/
int main(void)
{
    for (;;) {
        control();
    }

    //hal_init();
    //max7219_init();
    //acquisition_buffer[0] = 0;
    //greet_message();
    //acquisition_fill_buffer();
//
    //while(1)
    //{
        //for (uint8_t i = 1; i <= 10; ++i) {
            //bargraph_set_range(i, BARGRAPH_FILLED);
            //segment_display_num(i);
            //hal_delay_ms(i * 10);
        //}
        //for (int8_t i = 9; i >= 0; --i) {
            //bargraph_set_range(i, BARGRAPH_FILLED);
            //segment_display_num(i);
            //hal_delay_ms(i * 10);
        //}
        ////for (uint8_t i = 0; i < 100; ++i) {
            ////segment_display_num(i);
            ////hal_delay_ms(100);
        ////}
        ////for (int8_t i = 0; i <= BARGRAPH_SIZE; ++i) {
            ////bargraph_set_level(i, BARGRAPH_LEFT);
            ////hal_delay_ms(200);
        ////}
        ////
        ////for (int8_t i = BARGRAPH_SIZE - 1; i >= 0; --i) {
            ////bargraph_set_level(i, BARGRAPH_RIGHT);
            ////hal_delay_ms(200);
        ////}
        ////
        ////for (int8_t i = 1; i <= BARGRAPH_SIZE; ++i) {
            ////bargraph_set_level(i, BARGRAPH_RIGHT);
            ////hal_delay_ms(200);
        ////}
        ////
        ////for (int8_t i = BARGRAPH_SIZE - 1; i > 0; --i) {
            ////bargraph_set_level(i, BARGRAPH_LEFT);
            ////hal_delay_ms(200);
        ////}
    //}
}

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
