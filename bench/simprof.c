/*
This is free and unencumbered software released into the public domain.
For more information, please refer to <http://unlicense.org/>
*/

/**
 * @file   simprof.c
 * @brief  Per-stage cycle profiler. Runs a bench_profile.c image under simavr
 *         and reports the CPU cycles between successive PROFILE_MARK() writes to
 *         GPIOR0, i.e. the cost of each DSP stage. Prints one TSV line per stage
 *         transition: "<from_id>\t<to_id>\t<cycles>"; the driver
 *         (profile_stages.sh) maps the ids to stage names.
 *
 *         Same classic-AVRe+-core caveat as simrun.c: cycles are a consistent
 *         relative measure, not absolute avr64dd14 timing.
 *
 *   usage: simprof <firmware.elf> [mcu]
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
#define MARK_DONE   0xFFu
#define GPIOR0_ADDR 0x3E            /* atmega1284p GPIOR0, data-space address */
#define MAX_MARKS   64
#define MAX_CYCLES  (200ull * 1000ull * 1000ull)

/*---------------------------------------------------------------------------*/
/*                            LOCAL VARIABLES                                */
/*---------------------------------------------------------------------------*/
static struct { uint8_t id; avr_cycle_count_t cycle; } g_marks[MAX_MARKS];
static int g_n;
static int g_done;

/*---------------------------------------------------------------------------*/
/*                        FUNCTION IMPLEMENTATION                            */
/*---------------------------------------------------------------------------*/
static void marker_write(avr_t *avr, avr_io_addr_t addr, uint8_t v, void *param)
{
    (void)addr;
    (void)param;
    avr->data[GPIOR0_ADDR] = v;

    if (g_n < MAX_MARKS) {
        g_marks[g_n].id = v;
        g_marks[g_n].cycle = avr->cycle;
        ++g_n;
    }
    if (v == MARK_DONE)
        g_done = 1;
}

int main(int argc, char **argv)
{
    const char *mcu = "atmega1284p";
    elf_firmware_t fw;
    avr_t *avr;
    int i;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <firmware.elf> [mcu]\n", argv[0]);
        return 2;
    }
    if (argc >= 3)
        mcu = argv[2];

    memset(&fw, 0, sizeof(fw));
    if (elf_read_firmware(argv[1], &fw) != 0) {
        fprintf(stderr, "simprof: cannot read firmware %s\n", argv[1]);
        return 2;
    }
    avr = avr_make_mcu_by_name(mcu);
    if (!avr) {
        fprintf(stderr, "simprof: unknown mcu '%s'\n", mcu);
        return 2;
    }
    avr_init(avr);
    avr->frequency = 16000000;
    avr_load_firmware(avr, &fw);
    avr_register_io_write(avr, GPIOR0_ADDR, marker_write, NULL);

    while (!g_done && avr->cycle < MAX_CYCLES) {
        int state = avr_run(avr);
        if (state == cpu_Done || state == cpu_Crashed)
            break;
    }

    if (g_n < 2) {
        fprintf(stderr, "simprof: too few markers (%d) -- profiling build?\n", g_n);
        return 1;
    }

    /* One line per transition between successive markers. */
    for (i = 1; i < g_n; ++i)
        printf("%u\t%u\t%llu\n", g_marks[i - 1].id, g_marks[i].id,
               (unsigned long long)(g_marks[i].cycle - g_marks[i - 1].cycle));
    return 0;
}

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
