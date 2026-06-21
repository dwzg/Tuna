/*
This is free and unencumbered software released into the public domain.
For more information, please refer to <http://unlicense.org/>
*/

/**
 * @file   bench_window.c
 * @brief  Host benchmark comparing FFT-path pitch accuracy across window
 *         functions (src/analysis.c + src/fft.c, compiled with
 *         PITCH_METHOD_FFT). For each window it sweeps a corpus of synthetic
 *         tones - pure, harmonic-rich and weak-fundamental, clean and noisy -
 *         and reports the absolute pitch error in cents (mean / p95 / max) plus
 *         a count of gross (> 50 cent) misses, which flag octave-correction
 *         failures. It stands in its own window_apply_window so it can switch
 *         windows at run time; the firmware uses the equivalent precomputed Q15
 *         tables in src/window.c, so the comparison reflects the window math the
 *         target actually runs. Used to justify the default WINDOW_FUNCTION; the
 *         CTest wrapper additionally guards that the shipped default stays
 *         accurate.
 */

/*---------------------------------------------------------------------------*/
/*                               INCLUDES                                    */
/*---------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "config.h"
#include "analysis.h"

/*---------------------------------------------------------------------------*/
/*                         DEFINITIONS AND MACROS                            */
/*---------------------------------------------------------------------------*/
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Selectable windows, applied at run time by window_apply_window below. */
enum { WIN_DIRICHLET, WIN_HANNING, WIN_HAMMING, WIN_BLACKMAN, WIN_COUNT };

static const char *WIN_NAME[WIN_COUNT] = {
    "Dirichlet", "Hanning", "Hamming", "Blackman"
};

/* Which window the next analysis_fft_frequency() call should apply. */
static int g_window = WIN_HANNING;

/*---------------------------------------------------------------------------*/
/*                            LOCAL VARIABLES                                */
/*---------------------------------------------------------------------------*/
static uint32_t rng_state = 0x1234567u;

/*---------------------------------------------------------------------------*/
/*                        FUNCTION IMPLEMENTATION                            */
/*---------------------------------------------------------------------------*/

/* Deterministic uniform noise in [-1, 1) so runs are reproducible. */
static double noise(void)
{
    rng_state = rng_state * 1664525u + 1013904223u;
    return (double)(rng_state >> 8) / (double)(1u << 23) - 1.0;
}

/*
 * window.c stand-in. Mirrors the firmware's symmetric, half-table Q15 scaling
 * (coefficient applied to time_data[i] and its mirror, value >> 15), but
 * computes the coefficient from the standard window definition for g_window so
 * a single binary can compare every window.
 */
void window_apply_window(int16_t time_data[])
{
    uint16_t i;

    for (i = 0; i < FRAME_SIZE / 2; ++i) {
        double t = 2.0 * M_PI * i / (FRAME_SIZE - 1);
        double w;
        int32_t q15;

        switch (g_window) {
        case WIN_DIRICHLET: w = 1.0; break;
        case WIN_HANNING:   w = 0.5 - 0.5 * cos(t); break;
        case WIN_HAMMING:   w = 0.54 - 0.46 * cos(t); break;
        case WIN_BLACKMAN:  w = 0.42 - 0.5 * cos(t) + 0.08 * cos(2.0 * t); break;
        default:            w = 1.0; break;
        }

        q15 = (int32_t)lround(w * 32767.0);
        if (q15 > 32767) q15 = 32767;
        time_data[i] = (int16_t)(((int32_t)time_data[i] * q15) >> 15);
        time_data[FRAME_SIZE - i - 1] =
            (int16_t)(((int32_t)time_data[FRAME_SIZE - i - 1] * q15) >> 15);
    }
}

/* Synthesize a harmonic tone (12-bit ADC range), optionally noisy, and run the
 * FFT pitch detector. Returns the detected frequency in Hz. */
static double detect(double f0, const double *weights, int nh, double amp,
                     double noise_amp)
{
    int16_t x[FRAME_SIZE];
    int n, h;

    for (n = 0; n < FRAME_SIZE; ++n) {
        double s = 0.0;
        for (h = 1; h <= nh && f0 * h < SAMPLE_FREQ / 2.0; ++h)
            s += weights[h - 1] * amp * sin(2.0 * M_PI * f0 * h * n / SAMPLE_FREQ);
        s += noise_amp * noise();
        x[n] = (int16_t)(s > 2047 ? 2047 : s < -2048 ? -2048 : lround(s));
    }
    return analysis_fft_frequency(x);
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
    /* Harmonic profiles: pure tone, sawtooth-like (strong fundamental, 1/h
     * rolloff), and a weak/near-missing fundamental that leans on the octave
     * correction. */
    static const double pure[8] = { 1.0 };
    static const double saw[8]  = { 1.0, 1.0/2, 1.0/3, 1.0/4, 1.0/5, 1.0/6, 1.0/7, 1.0/8 };
    static const double weak[8] = { 0.15, 1.0, 0.9, 0.5, 0.3, 0.2, 0.15, 0.1 };
    struct { const char *name; const double *w; int nh; double fmax; } prof[] = {
        { "pure", pure, 1, 1700.0 },
        { "saw",  saw,  8,  900.0 },
        { "weak", weak, 8,  450.0 },
    };
    const double noise_levels[2] = { 0.0, 120.0 };  /* clean, then ~6% of full scale */

    /* Collect every case's |error| per window. */
    static double err[WIN_COUNT][4096];
    int count[WIN_COUNT];
    memset(count, 0, sizeof(count));

    for (int w = 0; w < WIN_COUNT; ++w) {
        g_window = w;
        for (size_t p = 0; p < sizeof(prof) / sizeof(prof[0]); ++p) {
            /* Scale so the summed harmonics fill ~90% of full scale without
             * clipping (bounded by the sum of harmonic magnitudes). */
            double sumw = 0.0;
            for (int h = 0; h < prof[p].nh; ++h) sumw += fabs(prof[p].w[h]);
            double amp = 0.9 * 2047.0 / sumw;
            for (int nl = 0; nl < 2; ++nl) {
                rng_state = 0x1234567u;  /* same noise draw across windows */
                for (double f = 80.0; f <= prof[p].fmax; f *= 1.18) {
                    double got = detect(f, prof[p].w, prof[p].nh, amp,
                                        noise_levels[nl]);
                    double cents = fabs(1200.0 * log2(got / f));
                    err[w][count[w]++] = cents;
                }
            }
        }
    }

    printf("Window benchmark  (FFT pitch path, FRAME_SIZE=%d, SAMPLE_FREQ=%lu, %d cases each)\n",
           FRAME_SIZE, (unsigned long)SAMPLE_FREQ, count[0]);
    printf("  %-10s  %9s  %9s  %9s  %12s\n",
           "window", "mean|c|", "p95|c|", "max|c|", "gross(>50c)");

    int best = 0;
    double best_mean = 1e9;
    for (int w = 0; w < WIN_COUNT; ++w) {
        double sum = 0.0;
        int gross = 0;
        for (int i = 0; i < count[w]; ++i) {
            sum += err[w][i];
            if (err[w][i] > 50.0) ++gross;
        }
        qsort(err[w], count[w], sizeof(double), cmp_double);
        double mean = sum / count[w];
        double p95 = err[w][(int)(0.95 * (count[w] - 1))];
        double max = err[w][count[w] - 1];
        printf("  %-10s  %9.2f  %9.2f  %9.2f  %12d%s\n",
               WIN_NAME[w], mean, p95, max, gross,
               w == WIN_HANNING ? "   <- default" : "");
        if (mean < best_mean) { best_mean = mean; best = w; }
    }
    printf("Best mean: %s\n", WIN_NAME[best]);

    /*
     * Regression guard on the shipped default only. Loose bounds: the corpus
     * includes deliberately hard weak-fundamental and noisy cases, so this
     * checks the default still works, not that it is flawless.
     */
    double sum = 0.0;
    int gross = 0;
    for (int i = 0; i < count[WIN_HANNING]; ++i) {
        sum += err[WIN_HANNING][i];
        if (err[WIN_HANNING][i] > 50.0) ++gross;
    }
    double mean = sum / count[WIN_HANNING];
    int fail = 0;
    if (mean > 5.0) {
        printf("FAIL: default window mean error %.2f c exceeds 5 c\n", mean);
        fail = 1;
    }
    if (gross > count[WIN_HANNING] / 10) {
        printf("FAIL: default window gross misses %d exceed 10%% of %d cases\n",
               gross, count[WIN_HANNING]);
        fail = 1;
    }
    printf("%s\n", fail ? "FAILED" : "PASSED");
    return fail;
}

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/
