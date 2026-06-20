/*
This is free and unencumbered software released into the public domain.
For more information, please refer to <http://unlicense.org/>
*/

/**
 * @file   test_yin.c
 * @brief  Host regression test for the YIN pitch estimator (src/yin.c). Builds
 *         synthetic harmonic tones at known fundamentals and checks that
 *         yin_frequency() recovers them. Runs natively (no AVR), so the
 *         fixed-point/integer behaviour matches the target bit for bit.
 */

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "config.h"
#include "yin.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int failures = 0;

/* Build a Hamming-tapered tone with up to 6 harmonics and run YIN on it. */
static double detect(double f0, const double *weights, int nh, double amp)
{
    int16_t x[FFT_SIZE];
    for (int n = 0; n < FFT_SIZE; ++n) {
        double w = 0.54 - 0.46 * cos(2.0 * M_PI * n / (FFT_SIZE - 1));
        double s = 0.0;
        for (int h = 1; h <= nh && f0 * h < SAMPLE_FREQ / 2.0; ++h)
            s += weights[h - 1] * amp * sin(2.0 * M_PI * f0 * h * n / SAMPLE_FREQ);
        s *= w;
        x[n] = (int16_t)(s > 2047 ? 2047 : s < -2048 ? -2048 : lround(s));
    }
    return yin_frequency(x);
}

static void expect_cents(const char *name, double got, double f0, double tol_cents)
{
    double cents = 1200.0 * log2(got / f0);
    int ok = fabs(cents) <= tol_cents;
    printf("  [%s] f0=%.2f got=%.2f (%+.1f cents) %s\n",
           name, f0, got, cents, ok ? "ok" : "FAIL");
    if (!ok) ++failures;
}

int main(void)
{
    const double pure[6]    = { 1.0, 0, 0, 0, 0, 0 };
    const double rich[6]    = { 1.0, 0.6, 0.4, 0.3, 0.2, 0.15 };
    const double weakfund[6]= { 0.2, 1.0, 0.7, 0.5, 0.3, 0.2 };

    /* Tolerances allow for YIN's inherent (mild) high-frequency bias at this
     * sample rate; they are tight enough to catch octave errors and algorithm
     * regressions. Frequencies span the usual instrument range. */
    puts("YIN: pure tones");
    for (double f = 60.0; f <= 500.0; f *= 1.5)
        expect_cents("pure", detect(f, pure, 1, 1800.0), f, 20.0);

    puts("YIN: harmonic-rich tones");
    for (double f = 55.0; f <= 400.0; f *= 1.6)
        expect_cents("rich", detect(f, rich, 6, 600.0), f, 25.0);

    puts("YIN: weak-fundamental tones (octave robustness)");
    for (double f = 70.0; f <= 350.0; f *= 1.5)
        expect_cents("weak", detect(f, weakfund, 6, 600.0), f, 25.0);

    printf("%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
