/*
This is free and unencumbered software released into the public domain.
For more information, please refer to <http://unlicense.org/>
*/

/**
 * @file   test_fft.c
 * @brief  Host regression test for the FFT pitch path (src/analysis.c +
 *         src/fft.c, compiled with PITCH_METHOD_FFT). Covers the real-input FFT
 *         frequency accuracy on pure tones and the HPS-style octave correction
 *         on weak/missing-fundamental tones. Runs natively (no AVR), so the
 *         fixed-point behaviour matches the target bit for bit. A simple Hamming
 *         window stub stands in for window.c so the test is self-contained.
 */

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "config.h"
#include "analysis.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int failures = 0;

/* window.c stand-in: the firmware applies its window before the FFT; here a
 * plain Hamming keeps the test self-contained and deterministic. */
void window_apply_window(int16_t a[])
{
    for (int n = 0; n < FFT_SIZE; ++n) {
        double w = 0.54 - 0.46 * cos(2.0 * M_PI * n / (FFT_SIZE - 1));
        a[n] = (int16_t)lround(a[n] * w);
    }
}

static double detect(double f0, const double *weights, int nh, double amp)
{
    int16_t x[FFT_SIZE];
    for (int n = 0; n < FFT_SIZE; ++n) {
        double s = 0.0;
        for (int h = 1; h <= nh && f0 * h < SAMPLE_FREQ / 2.0; ++h)
            s += weights[h - 1] * amp * sin(2.0 * M_PI * f0 * h * n / SAMPLE_FREQ);
        x[n] = (int16_t)(s > 2047 ? 2047 : s < -2048 ? -2048 : lround(s));
    }
    return analysis_fft_frequency(x);
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
    const double weakfund[6]= { 0.15, 1.0, 0.9, 0.5, 0.3, 0.2 };

    puts("FFT: pure-tone accuracy (real-input transform + parabolic interp)");
    for (double f = 90.0; f <= 1700.0; f *= 1.7)
        expect_cents("pure", detect(f, pure, 1, 1800.0), f, 10.0);

    puts("FFT: weak/missing-fundamental (octave correction)");
    for (double f = 75.0; f <= 400.0; f *= 1.4)
        expect_cents("weak", detect(f, weakfund, 6, 600.0), f, 25.0);

    printf("%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
