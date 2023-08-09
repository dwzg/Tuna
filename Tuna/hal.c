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
 * @file   hal.c
 * @author Dennis Witzig
 * @date   2022-09-30
 * @brief  This module contains the hardware specific code for Tuna.
 */

/*---------------------------------------------------------------------------*/
/*                          INCLUDE DEFINITIONS                              */
/*---------------------------------------------------------------------------*/
#define F_CPU 24000000UL
#define __DELAY_BACKWARD_COMPATIBLE__

/*---------------------------------------------------------------------------*/
/*                               INCLUDES                                    */
/*---------------------------------------------------------------------------*/
#include <stdint.h>
#include <xc.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "config.h"
#include "hal.h"

/*---------------------------------------------------------------------------*/
/*                         DEFINITIONS AND MACROS                            */
/*---------------------------------------------------------------------------*/
#define OUTPUT 0
#define INPUT 1

#define LOW 0
#define HIGH 1

#define COUNTER_TOP_VALUE (((F_CPU)/(SAMPLE_FREQ)) - 1UL)

#define DIN_PORT &PORTC
#define DIN_PIN PIN1_bp

#define CLK_PORT &PORTC
#define CLK_PIN PIN2_bp

#define LOAD_PORT &PORTC
#define LOAD_PIN PIN3_bp

/*---------------------------------------------------------------------------*/
/*                         TYPEDEFS AND STRUCTURES                           */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                               PROTOTYPES                                  */
/*---------------------------------------------------------------------------*/
void init_pin(PORT_t *port, uint8_t pin, uint8_t dir);
void set_pin(PORT_t *port, uint8_t pin, uint8_t value);

/*---------------------------------------------------------------------------*/
/*                            LOCAL VARIABLES                                */
/*---------------------------------------------------------------------------*/
HAL_SAMPLE_COUNTER_CALLBACK sample_counter_callback;

/*---------------------------------------------------------------------------*/
/*                        FUNCTION IMPLEMENTATION                            */
/*---------------------------------------------------------------------------*/
void hal_init()
{
    cli();

    /**
     * @brief Set clock frequency of internal high frequency oscillator to 24 MHz
     *        with auto-tuning enabled.
     */
    _PROTECTED_WRITE(CLKCTRL.OSCHFCTRLA, ((CLKCTRL_FRQSEL_24M_gc) | (CLKCTRL_AUTOTUNE_bm)));

    /**
     * @brief Set internal voltage reference to VDD (5 V).
     */
    VREF.ADC0REF = VREF_REFSEL_VDD_gc;

    /**
     * @brief Set ADC to differential 12 bit mode.
     */
    ADC0.CTRLA = ADC_ENABLE_bm
               | ADC_RESSEL_12BIT_gc;
    ADC0.MUXPOS = ADC_MUXPOS_AIN4_gc;

    /**
     * @brief Initialize counter for sample frequency
     */
    TCA0.SINGLE.PER = COUNTER_TOP_VALUE;
    TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;

    /**
     * @brief Initialize pins used by MAX7219 display driver.
     */
    init_pin(DIN_PORT, DIN_PIN, OUTPUT);
    init_pin(CLK_PORT, CLK_PIN, OUTPUT);
    init_pin(LOAD_PORT, LOAD_PIN, OUTPUT);

    sei();
}

int16_t hal_get_adc_sample()
{
    int16_t sample;

    ADC0.COMMAND = ADC_STCONV_bm;

    while (!(ADC0.INTFLAGS & ADC_RESRDY_bm));

    sample = ((int16_t)ADC0.RES) - (1 << 11);

    return sample;
}

void hal_start_sample_counter(HAL_SAMPLE_COUNTER_CALLBACK callback)
{
    sample_counter_callback = callback;

    TCA0.SINGLE.CTRLA |= TCA_SINGLE_ENABLE_bm;
}

void hal_stop_sample_counter()
{
    TCA0.SINGLE.CTRLA &= ~(TCA_SINGLE_ENABLE_bm);
    TCA0.SINGLE.CNT = 0;
}

void hal_set_din(uint8_t value)
{
    set_pin(DIN_PORT, DIN_PIN, value);
}

void hal_set_clk(uint8_t value)
{
    set_pin(CLK_PORT, CLK_PIN, value);
}

void hal_set_load(uint8_t value)
{
    set_pin(LOAD_PORT, LOAD_PIN, value);
}

void hal_delay_ms(uint16_t ms)
{
    _delay_ms((double)ms);
}

void hal_delay_us(uint16_t us)
{
    _delay_us((double)us);
}

void init_pin(PORT_t *port, uint8_t pin, uint8_t dir)
{
    if (dir == OUTPUT) {
        port->DIRSET |= (1 << pin);
        } else {
        port->DIRCLR |= (1 << pin);
    }
}

void set_pin(PORT_t *port, uint8_t pin, uint8_t value)
{
    if (value == HIGH) {
        port->OUTSET |= (1 << pin);
        } else {
        port->OUTCLR |= (1 << pin);
    }
}

ISR(TCA0_OVF_vect)
{
    TCA0.SINGLE.INTFLAGS |= TCA_SINGLE_OVF_bm;
    sample_counter_callback();
}

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
