/*
This is free and unencumbered software released into the public domain.
For more information, please refer to <http://unlicense.org/>
*/

/**
 * @file   test_smoothing.c
 * @brief  Host regression test for the frequency stabiliser (src/smoothing.c).
 *         Exercises the silence reset, the EMA hold, transient octave-error
 *         rejection (with eventual give-in), and the snap-through on a genuine
 *         note change. Assumes the config.h defaults SMOOTHING_ALPHA=0.5 and
 *         OCTAVE_JUMP_FRAMES=3.
 */

#include <stdio.h>
#include <math.h>
#include "config.h"
#include "smoothing.h"

static int failures = 0;

static void expect(const char *name, double got, double want, double tol)
{
    int ok = fabs(got - want) <= tol;
    printf("  [%s] got=%.3f want=%.3f %s\n", name, got, want, ok ? "ok" : "FAIL");
    if (!ok) ++failures;
}

int main(void)
{
    double y;

    /* Silence resets the filter and reports nothing. */
    expect("silence", smooth_frequency(0.0), 0.0, 1e-9);

    /* First estimate after silence passes straight through. */
    expect("first", smooth_frequency(100.0), 100.0, 1e-9);

    /* A held note within ~half a semitone (<3%) is EMA-smoothed (alpha=0.5). */
    expect("ema", smooth_frequency(101.0), 0.5 * 101.0 + 0.5 * 100.0, 1e-9);

    /* A genuine, non-octave change of >~half a semitone snaps through. */
    smooth_frequency(0.0);
    smooth_frequency(100.0);
    expect("snap", smooth_frequency(130.0), 130.0, 1e-9);

    /* A sudden octave jump is rejected until it persists OCTAVE_JUMP_FRAMES frames. */
    smooth_frequency(0.0);
    smooth_frequency(100.0);
    y = smooth_frequency(200.0);            /* vote 1: held */
    expect("octave-hold-1", y, 100.0, 1e-9);
    y = smooth_frequency(200.0);            /* vote 2: held */
    expect("octave-hold-2", y, 100.0, 1e-9);
    y = smooth_frequency(200.0);            /* vote 3: accepted */
    expect("octave-give-in", y, 200.0, 1e-9);

    /* The downward half-pitch error is rejected symmetrically. */
    smooth_frequency(0.0);
    smooth_frequency(200.0);
    y = smooth_frequency(100.0);            /* vote 1: held */
    expect("octave-down-hold-1", y, 200.0, 1e-9);
    y = smooth_frequency(100.0);            /* vote 2: held */
    expect("octave-down-hold-2", y, 200.0, 1e-9);
    y = smooth_frequency(100.0);            /* vote 3: accepted */
    expect("octave-down-give-in", y, 100.0, 1e-9);

    /* A non-octave interval (here a fifth) never counts as an octave artifact
     * and snaps through immediately. */
    smooth_frequency(0.0);
    smooth_frequency(100.0);
    expect("snap-fifth", smooth_frequency(150.0), 150.0, 1e-9);

    /* Silence resets the pending octave votes: the same octave seen again
     * after a reset starts over as a fresh first estimate. */
    smooth_frequency(0.0);
    smooth_frequency(100.0);
    smooth_frequency(200.0);                /* vote 1 */
    smooth_frequency(200.0);                /* vote 2 */
    smooth_frequency(0.0);                  /* reset */
    expect("votes-reset", smooth_frequency(200.0), 200.0, 1e-9);

    printf("%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
