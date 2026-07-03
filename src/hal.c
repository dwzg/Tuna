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

/*
 * ADC oversampling factor. The ADC is timer-paced at OVERSAMPLE_FACTOR times
 * SAMPLE_FREQ and each delivered sample is the average of that many
 * conversions. There is no analog anti-aliasing filter in front of the ADC
 * beyond the AC coupling, so at a bare SAMPLE_FREQ everything above Nyquist
 * (upper harmonics of the note itself) would fold into the analysis band. The
 * boxcar average of evenly spaced conversions is a free first-order comb
 * filter with nulls at multiples of SAMPLE_FREQ, attenuating the fold-back
 * region, and the averaging also lowers the ADC noise floor.
 */
#define OVERSAMPLE_FACTOR 4

#define OVERSAMPLE_RATE ((SAMPLE_FREQ) * (OVERSAMPLE_FACTOR))

/* Rounded division: every reported frequency scales with the real conversion
 * rate, and truncating here would run ~580 ppm (~1 cent) sharp at 4x4096 Hz;
 * rounding keeps the rate within ~110 ppm (~0.2 cents) of nominal. */
#define COUNTER_TOP_VALUE ((((F_CPU) + (OVERSAMPLE_RATE) / 2) / (OVERSAMPLE_RATE)) - 1UL)

/*
 * Mid-scale of the 12 bit single-ended ADC result (0..4095). Subtracting it
 * re-centres the AC-coupled, VDD/2-biased conversion to a signed -2048..2047.
 */
#define ADC_ZERO_OFFSET (1 << 11)

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
static void init_pin(PORT_t *port, uint8_t pin, uint8_t dir);
static void set_pin(PORT_t *port, uint8_t pin, uint8_t value);

/*---------------------------------------------------------------------------*/
/*                            LOCAL VARIABLES                                */
/*---------------------------------------------------------------------------*/
static HAL_SAMPLE_COUNTER_CALLBACK sample_counter_callback;

/* Oversampling decimator state. Touched only by the result-ready ISR while
 * sampling runs, and reset by hal_start_sample_counter() while it does not. */
static volatile int16_t oversample_acc;
static volatile uint8_t oversample_count;

/*---------------------------------------------------------------------------*/
/*                        FUNCTION IMPLEMENTATION                            */
/*---------------------------------------------------------------------------*/
void hal_init(void)
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
     *        to a signed value by subtracting ADC_ZERO_OFFSET as each result is
     *        read in the result-ready ISR.
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
     * @brief Initialize pins used by MAX7219 display driver.
     *        The display is bit-banged on these GPIOs: the manufactured board
     *        wires DIN/CLK/LOAD to PC1/PC2/PC3, and no SPI0/USART pin-mux on the
     *        AVR64DD14 can put a hardware shift clock on PC2 while driving data
     *        on PC1, so a hardware serial peripheral cannot be used here.
     */
    init_pin(DIN_PORT, DIN_PIN, OUTPUT);
    init_pin(CLK_PORT, CLK_PIN, OUTPUT);
    init_pin(LOAD_PORT, LOAD_PIN, OUTPUT);

    sei();
}

void hal_start_sample_counter(HAL_SAMPLE_COUNTER_CALLBACK callback)
{
    sample_counter_callback = callback;
    oversample_acc = 0;
    oversample_count = 0;

    /* Arm the result-ready interrupt, then let the timer drive conversions. */
    ADC0.INTFLAGS = ADC_RESRDY_bm;
    ADC0.INTCTRL = ADC_RESRDY_bm;

    TCA0.SINGLE.CTRLA |= TCA_SINGLE_ENABLE_bm;
}

void hal_stop_sample_counter(void)
{
    TCA0.SINGLE.CTRLA &= ~(TCA_SINGLE_ENABLE_bm);
    TCA0.SINGLE.CNT = 0;

    ADC0.INTCTRL = 0;
    ADC0.INTFLAGS = ADC_RESRDY_bm;
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

/*
 * DIRSET/DIRCLR/OUTSET/OUTCLR are write-1-to-strobe registers that read back
 * as DIR/OUT, so they must be written with plain assignment: a read-modify-
 * write like OUTCLR |= bit would write back every currently-set OUT bit and
 * clear all high pins on the port, not just the target pin.
 */
static void init_pin(PORT_t *port, uint8_t pin, uint8_t dir)
{
    if (dir == OUTPUT) {
        port->DIRSET = (uint8_t)(1 << pin);
    } else {
        port->DIRCLR = (uint8_t)(1 << pin);
    }
}

static void set_pin(PORT_t *port, uint8_t pin, uint8_t value)
{
    if (value == HIGH) {
        port->OUTSET = (uint8_t)(1 << pin);
    } else {
        port->OUTCLR = (uint8_t)(1 << pin);
    }
}

ISR(ADC0_RESRDY_vect)
{
    /* Reading the result register clears the result-ready flag. The conversion
     * was started in hardware by the timer overflow event. Conversions run at
     * OVERSAMPLE_FACTOR times the sample rate; each group is averaged and
     * delivered as one sample (see OVERSAMPLE_FACTOR above). The accumulator
     * cannot overflow: OVERSAMPLE_FACTOR * 2048 fits comfortably in int16_t. */
    oversample_acc += ((int16_t)ADC0.RES) - ADC_ZERO_OFFSET;

    if (++oversample_count < OVERSAMPLE_FACTOR) {
        return;
    }

    {
        int16_t sample = (int16_t)(oversample_acc / OVERSAMPLE_FACTOR);

        oversample_acc = 0;
        oversample_count = 0;

        sample_counter_callback(sample);
    }
}

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
