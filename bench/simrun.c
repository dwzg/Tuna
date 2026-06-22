/*
This is free and unencumbered software released into the public domain.
For more information, please refer to <http://unlicense.org/>
*/

/**
 * @file   simrun.c
 * @brief  Host program that runs a bench_dsp.c firmware image under simavr and
 *         prints the number of CPU cycles spent in the measured region.
 *
 *         The firmware brackets its DSP call with writes of MARK_START / MARK_END
 *         to GPIOR0. We register an I/O write callback on GPIOR0's data-space
 *         address, latch the simavr cycle counter at each marker, and report the
 *         difference. simavr has no AVR-Dx (AVRxt) core, so the harness runs on
 *         an atmega1284p (classic AVRe+ core) with ample SRAM; the resulting
 *         cycle counts are a consistent proxy for comparing optimization levels,
 *         not an absolute timing of the avr64dd14. See bench/README.md.
 *
 *   usage: simrun <firmware.elf> [mcu]
 */

/*---------------------------------------------------------------------------*/
/*                               INCLUDES                                    */
/*---------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <simavr/sim_avr.h>
#include <simavr/sim_elf.h>
#include <simavr/sim_io.h>

/*---------------------------------------------------------------------------*/
/*                         DEFINITIONS AND MACROS                            */
/*---------------------------------------------------------------------------*/
#define MARK_START 1u
#define MARK_END   2u

/* GPIOR0 on the atmega1284p: I/O 0x1E -> data-space 0x1E + 0x20 = 0x3E. */
#define GPIOR0_ADDR 0x3E

/* Safety cap so a runaway image cannot hang the runner. */
#define MAX_CYCLES  (200ull * 1000ull * 1000ull)

/*---------------------------------------------------------------------------*/
/*                            LOCAL VARIABLES                                */
/*---------------------------------------------------------------------------*/
static avr_cycle_count_t g_start;
static avr_cycle_count_t g_end;
static int g_done;

/*---------------------------------------------------------------------------*/
/*                        FUNCTION IMPLEMENTATION                            */
/*---------------------------------------------------------------------------*/
static void marker_write(avr_t *avr, avr_io_addr_t addr, uint8_t v, void *param)
{
    (void)addr;
    (void)param;
    avr->data[GPIOR0_ADDR] = v;     /* the callback owns the store */

    if (v == MARK_START) {
        g_start = avr->cycle;
    } else if (v == MARK_END) {
        g_end = avr->cycle;
        g_done = 1;
    }
}

int main(int argc, char **argv)
{
    const char *mcu = "atmega1284p";
    elf_firmware_t fw;
    avr_t *avr;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <firmware.elf> [mcu]\n", argv[0]);
        return 2;
    }
    if (argc >= 3)
        mcu = argv[2];

    memset(&fw, 0, sizeof(fw));
    if (elf_read_firmware(argv[1], &fw) != 0) {
        fprintf(stderr, "simrun: cannot read firmware %s\n", argv[1]);
        return 2;
    }

    avr = avr_make_mcu_by_name(mcu);
    if (!avr) {
        fprintf(stderr, "simrun: unknown mcu '%s'\n", mcu);
        return 2;
    }
    avr_init(avr);
    avr->frequency = 16000000;      /* cycle counting is frequency-independent */
    avr_load_firmware(avr, &fw);

    avr_register_io_write(avr, GPIOR0_ADDR, marker_write, NULL);

    while (!g_done && avr->cycle < MAX_CYCLES) {
        int state = avr_run(avr);
        if (state == cpu_Done || state == cpu_Crashed) {
            fprintf(stderr, "simrun: cpu stopped (state %d) before MARK_END\n", state);
            break;
        }
    }

    if (!g_done) {
        fprintf(stderr, "simrun: MARK_END never reached (cycles=%llu)\n",
                (unsigned long long)avr->cycle);
        return 1;
    }

    printf("%llu\n", (unsigned long long)(g_end - g_start));
    return 0;
}

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
