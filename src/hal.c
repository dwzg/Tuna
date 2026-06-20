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
#ifndef F_CPU
#define F_CPU 24000000UL
#endif
#define __DELAY_BACKWARD_COMPATIBLE__

/*---------------------------------------------------------------------------*/
/*                               INCLUDES                                    */
/*---------------------------------------------------------------------------*/
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
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

/*
 * MAX7219 link. The serial data and clock are driven by the hardware SPI0
 * peripheral (PORTMUX ALT6: MOSI=PC1, SCK=PC3); only the load/latch line is a
 * plain GPIO, toggled manually around each 16 bit frame. PC1 keeps its original
 * role as the data line, so wiring-wise only the clock (PC2 -> PC3) and load
 * (PC3 -> PC2) lines swap relative to the former bit-banged pinout, and the
 * display stays on the MVIO-capable PORTC.
 *
 * SPI0 SS (ALT6: PF7) is unused: SPI_SSD_bm frees it so master mode is never
 * disturbed and the pin stays available for UPDI.
 */
#define LOAD_PORT &PORTC
#define LOAD_PIN PIN2_bp

#define SPI_MOSI_bm PIN1_bm
#define SPI_SCK_bm PIN3_bm

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
     * @brief Set ADC reference to VDD (5 V).
     */
    VREF.ADC0REF = VREF_REFSEL_VDD_gc;

    /**
     * @brief Set ADC clock to CLK_PER / 16 = 1.5 MHz (at the device maximum).
     *        Without this the ADC runs at the default prescaler, far above
     *        the specified maximum ADC clock at 24 MHz CLK_PER, which yields
     *        inaccurate conversions.
     */
    ADC0.CTRLC = ADC_PRESC_DIV16_gc;

    /**
     * @brief Extend the sample duration to settle higher source impedances.
     */
    ADC0.SAMPCTRL = 14;

    /**
     * @brief Enable the ADC in single-ended 12 bit mode (CONVMODE defaults to
     *        single-ended). The AC-coupled, VDD/2-biased input is re-centred
     *        to a signed value in hal_get_adc_sample().
     */
    ADC0.CTRLA = ADC_ENABLE_bm
               | ADC_RESSEL_12BIT_gc;
    ADC0.MUXPOS = ADC_MUXPOS_AIN4_gc;

    /**
     * @brief Initialize counter for sample frequency. The overflow is routed
     *        through the event system (below) rather than raising an interrupt,
     *        so no TCA overflow ISR is enabled here.
     */
    TCA0.SINGLE.PER = COUNTER_TOP_VALUE;

    /**
     * @brief Route the TCA0 overflow to the ADC start trigger via an event
     *        channel. Each conversion is then started directly in hardware at
     *        the timer overflow, eliminating the ISR-latency jitter of starting
     *        the conversion from software. The completed result is collected in
     *        the ADC result-ready ISR (enabled while sampling is active).
     */
    EVSYS.CHANNEL0 = EVSYS_CHANNEL0_TCA0_OVF_LUNF_gc;
    EVSYS.USERADC0START = EVSYS_USER_CHANNEL0_gc;
    ADC0.EVCTRL = ADC_STARTEI_bm;

    /**
     * @brief Route SPI0 to the MAX7219 pins and configure it as a host. The
     *        MAX7219 clocks data in MSB-first on the rising edge with the clock
     *        idle low (SPI mode 0). CLK_PER/4 = 6 MHz stays below the part's
     *        10 MHz limit. SPI_SSD_bm disables the hardware slave-select so the
     *        unused SS pin (PF7) cannot knock SPI0 out of host mode; the load
     *        line is driven by hand instead (see max7219_write()).
     */
    PORTMUX.SPIROUTEA = (PORTMUX.SPIROUTEA & ~PORTMUX_SPI0_gm) | PORTMUX_SPI0_ALT6_gc;
    PORTC.DIRSET = SPI_MOSI_bm | SPI_SCK_bm;
    init_pin(LOAD_PORT, LOAD_PIN, OUTPUT);

    SPI0.CTRLB = SPI_SSD_bm | SPI_MODE_0_gc;
    SPI0.CTRLA = SPI_ENABLE_bm | SPI_MASTER_bm | SPI_PRESC_DIV4_gc;

    sei();
}

int16_t hal_get_adc_sample()
{
    int16_t sample;

    ADC0.COMMAND = ADC_STCONV_bm;

    while (!(ADC0.INTFLAGS & ADC_RESRDY_bm));

    /* Single-ended 12 bit result is 0..4095; re-centre to -2048..2047. */
    sample = ((int16_t)ADC0.RES) - (1 << 11);

    return sample;
}

void hal_start_sample_counter(HAL_SAMPLE_COUNTER_CALLBACK callback)
{
    sample_counter_callback = callback;

    /* Arm the result-ready interrupt, then let the timer drive conversions. */
    ADC0.INTFLAGS = ADC_RESRDY_bm;
    ADC0.INTCTRL = ADC_RESRDY_bm;

    TCA0.SINGLE.CTRLA |= TCA_SINGLE_ENABLE_bm;
}

void hal_stop_sample_counter()
{
    TCA0.SINGLE.CTRLA &= ~(TCA_SINGLE_ENABLE_bm);
    TCA0.SINGLE.CNT = 0;

    ADC0.INTCTRL = 0;
    ADC0.INTFLAGS = ADC_RESRDY_bm;
}

void hal_spi_write(uint8_t value)
{
    SPI0.DATA = value;

    /* Wait for the transfer to finish; reading DATA clears the interrupt flag. */
    while (!(SPI0.INTFLAGS & SPI_IF_bm));

    (void)SPI0.DATA;
}

void hal_set_load(uint8_t value)
{
    set_pin(LOAD_PORT, LOAD_PIN, value);
}

void hal_sleep_idle(void)
{
    /*
     * Idle sleep stops the CPU clock but leaves the peripheral clock running,
     * so TCA0 keeps pacing conversions, the ADC keeps sampling and the
     * result-ready ISR still fires to wake the core. Used to spend the ~250 ms
     * acquisition window asleep instead of spinning.
     */
    set_sleep_mode(SLEEP_MODE_IDLE);
    sleep_enable();
    sleep_cpu();
    sleep_disable();
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

ISR(ADC0_RESRDY_vect)
{
    /* Reading the result register clears the result-ready flag. The conversion
     * was started in hardware by the timer overflow event. */
    int16_t sample = ((int16_t)ADC0.RES) - (1 << 11);

    sample_counter_callback(sample);
}

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
