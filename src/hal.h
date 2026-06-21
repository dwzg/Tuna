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
 * @file   hal.h
 * @author Dennis Witzig
 * @date   2022-09-30
 * @brief  This module contains the hardware specific header for Tuna.
 */

#ifndef HAL_H_
#define HAL_H_

/*---------------------------------------------------------------------------*/
/*                               INCLUDES                                    */
/*---------------------------------------------------------------------------*/
#include <stdint.h>

/*---------------------------------------------------------------------------*/
/*                         DEFINITIONS AND MACROS                            */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                         TYPEDEFS AND STRUCTURES                           */
/*---------------------------------------------------------------------------*/
typedef void (*HAL_SAMPLE_COUNTER_CALLBACK)(int16_t sample);

/*---------------------------------------------------------------------------*/
/*                            GLOBAL VARIABLES                               */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                           FUNCTION PROTOTYPES                             */
/*---------------------------------------------------------------------------*/
void hal_init(void);
void hal_start_sample_counter(HAL_SAMPLE_COUNTER_CALLBACK callback);
void hal_stop_sample_counter(void);
void hal_set_din(uint8_t value);
void hal_set_clk(uint8_t value);
void hal_set_load(uint8_t value);
void hal_sleep_idle(void);

/**
 * @brief Globally disable / re-enable interrupts to bracket a short critical
 *        section. Used to read multi-byte state shared with an ISR atomically on
 *        this 8-bit core. They assume interrupts are enabled in normal operation
 *        (hal_init() leaves them so): hal_enable_interrupts() unconditionally
 *        re-enables, it does not save and restore the prior state, so do not nest
 *        these or call them from within an ISR.
 */
void hal_disable_interrupts(void);
void hal_enable_interrupts(void);

void hal_delay_ms(uint16_t ms);
void hal_delay_us(uint16_t us);

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
#endif /* HAL_H_ */
