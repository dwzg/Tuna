/*
This is free and unencumbered software released into the public domain.
For more information, please refer to <http://unlicense.org/>
*/

/**
 * @file   sweep_yin.c
 * @brief  Host accuracy probe for the YIN parameter sweep. Runs a corpus of
 *         synthetic tones spanning the instrument range (deliberately down to
 *         30 Hz so that shrinking YIN_TAU_MAX visibly drops the low notes) and
 *         reports the pitch error in cents. yin.c is compiled with whatever
 *         YIN_TAU_MAX / YIN_W the build defines, so sweep.sh can pair these
 *         accuracy numbers with the matching simavr cycle cost.
 *
 *         Prints one machine-readable line to stdout:
 *           <median|c> <p95|c> <gross(>50c)> <count> <lowest_ok_hz>
 *         The median (not mean) is reported because a few octave/failure cases
 *         carry huge errors that would swamp the mean; the median tracks the
 *         in-range precision while the gross count tracks range/robustness.
 *         lowest_ok_hz is the lowest swept f0 still within 50 cents (a proxy
 *         for the usable low-frequency floor).
 */

/*---------------------------------------------------------------------------*/
/*                               INCLUDES                                    */
/*---------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "config.h"
#include "yin.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*---------------------------------------------------------------------------*/
/*                        FUNCTION IMPLEMENTATION                            */
/*---------------------------------------------------------------------------*/
/* Hamming-tapered harmonic tone (12-bit range), then YIN. */
static double detect(double f0, const double *weights, int nh, double amp)
{
    int16_t x[FRAME_SIZE];
    int n, h;

    for (n = 0; n < FRAME_SIZE; ++n) {
        double w = 0.54 - 0.46 * cos(2.0 * M_PI * n / (FRAME_SIZE - 1));
        double s = 0.0;
        for (h = 1; h <= nh && f0 * h < SAMPLE_FREQ / 2.0; ++h)
            s += weights[h - 1] * amp * sin(2.0 * M_PI * f0 * h * n / SAMPLE_FREQ);
        s *= w;
        x[n] = (int16_t)(s > 2047 ? 2047 : s < -2048 ? -2048 : lround(s));
    }
    return yin_frequency(x);
}

static int cmp_double(const void *a, const void *b)
{
    double da = *(const double *)a, db = *(const double *)b;
    return (da > db) - (da < db);
}

/*---------------------------------------------------------------------------*/
/*                                  MAIN                                     */
/*---------------------------------------------------------------------------*/
int main(void)
{
    static const double pure[6] = { 1.0, 0, 0, 0, 0, 0 };
    static const double rich[6] = { 1.0, 0.6, 0.4, 0.3, 0.2, 0.15 };
    static const double weak[6] = { 0.2, 1.0, 0.7, 0.5, 0.3, 0.2 };
    struct { const double *w; int nh; double amp; } prof[] = {
        { pure, 1, 1800.0 }, { rich, 6, 600.0 }, { weak, 6, 600.0 },
    };

    static double err[8192];
    int count = 0;
    double lowest_ok = 1e9;
    size_t p;

    for (p = 0; p < sizeof(prof) / sizeof(prof[0]); ++p) {
        double f;
        for (f = 30.0; f <= 1000.0; f *= 1.10) {
            double got = detect(f, prof[p].w, prof[p].nh, prof[p].amp);
            double cents = (got > 0.0) ? fabs(1200.0 * log2(got / f)) : 9999.0;
            err[count++] = cents;
            if (cents <= 50.0 && f < lowest_ok)
                lowest_ok = f;
        }
    }

    double sum = 0.0;
    int gross = 0;
    int i;
    for (i = 0; i < count; ++i) {
        sum += err[i];
        if (err[i] > 50.0) ++gross;
    }
    (void)sum;
    qsort(err, count, sizeof(double), cmp_double);

    printf("%.2f %.2f %d %d %.0f\n",
           err[(int)(0.50 * (count - 1))], err[(int)(0.95 * (count - 1))],
           gross, count, lowest_ok);
    return 0;
}

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
