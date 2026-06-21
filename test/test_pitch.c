/*
This is free and unencumbered software released into the public domain.
For more information, please refer to <http://unlicense.org/>
*/

/**
 * @file   test_pitch.c
 * @brief  Host regression test for the note mapper (src/pitch.c). Checks that
 *         pitch_from_frequency() maps known frequencies to the right pitch
 *         class/octave with the expected cents deviation, and rejects
 *         out-of-range input. The default (SHARP) accidental convention is
 *         assumed, matching config.h.
 */

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "config.h"
#include "pitch.h"

static int failures = 0;

static void expect_note(const char *name, double freq, PITCH_CLASS pc, int8_t octave,
                        double cents_lo, double cents_hi)
{
    NOTE n = pitch_from_frequency(freq);
    int ok = n.valid && n.pitch_class == pc && n.octave == octave
          && n.cents >= cents_lo && n.cents <= cents_hi;
    printf("  [%s] f=%.2f -> valid=%u pc=%d oct=%d cents=%+.1f %s\n",
           name, freq, n.valid, (int)n.pitch_class, (int)n.octave, n.cents,
           ok ? "ok" : "FAIL");
    if (!ok) ++failures;
}

static void expect_invalid(const char *name, double freq)
{
    NOTE n = pitch_from_frequency(freq);
    int ok = !n.valid;
    printf("  [%s] f=%.2f -> valid=%u %s\n", name, freq, n.valid, ok ? "ok" : "FAIL");
    if (!ok) ++failures;
}

int main(void)
{
    puts("pitch: exact equal-tempered references (A4=440)");
    expect_note("A4",  440.00, A, 4, -1.0, 1.0);
    expect_note("C4",  261.63, C, 4, -1.0, 1.0);
    expect_note("C0",   16.35, C, 0, -1.0, 1.0);
    expect_note("A2",  110.00, A, 2, -1.0, 1.0);
    expect_note("H4",  493.88, H, 4, -1.0, 1.0);

    puts("pitch: cents deviation sign and magnitude");
    /* 445 Hz is ~+19.6 cents sharp of A4. */
    expect_note("A4+sharp", 445.00, A, 4, 15.0, 24.0);
    /* 435 Hz is ~-19.8 cents flat of A4. */
    expect_note("A4-flat",  435.00, A, 4, -24.0, -15.0);

    puts("pitch: out-of-range / silence rejected");
    expect_invalid("zero",     0.0);
    expect_invalid("negative", -100.0);
    expect_invalid("subaudio", 5.0);     /* below octave 0 */
    expect_invalid("ultra",    9000.0);  /* above the table */

    printf("%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
