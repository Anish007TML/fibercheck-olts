/* sim_meter.c - PC simulation of the FiberCheck front end + firmware DSP.
 *
 * Compares two ways of reading low optical power:
 *   DC   : CW light, block mean minus a stored zero (dark) reading
 *   TONE : light switched on/off at 2 kHz, Goertzel amplitude at 2 kHz
 *
 * Impairments modelled at the TIA output (assumed, typical-order values):
 *   - offset drift since the last zero: uniform +/-1 mV
 *     (dark current, ambient light and temperature; = +/-100 pA on 10 Mohm)
 *   - mains pickup: 2 mV at 49.7-50.3 Hz + 0.5 mV third harmonic
 *   - white noise: 150 uV rms (TIA + ADC)
 *   - ADC: 12-bit, 16x hardware oversampling -> 16-bit codes over a 3.0 V reference
 * Block: fs = 32 kHz, N = 3200 samples (100 ms). 300 trials per level.
 *
 * Build: make sim   (from the repository root)
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "meter_dsp.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define FS 32000.0f
#define N  3200
#define TRIALS 300
#define VREF 0.100f            /* TIA output idles at 100 mV (single supply) */

static double urand(void) { return rand() / (RAND_MAX + 1.0); }
static double gauss(void)
{
    double u1 = urand() + 1e-12, u2 = urand();
    return sqrt(-2 * log(u1)) * cos(2 * M_PI * u2);
}

static int pick_range(float v_on_10M)  /* settle auto-range from range 0 */
{
    int r = 0, prev = -1;
    while (r != prev) { prev = r; r = autorange(r, v_on_10M / powf(10, r)); }
    return r;
}

static float x[N];

static void make_block(float p_dbm, int modulated, int range, float off)
{
    float r_a_w = responsivity(1310);
    float i_on  = r_a_w * 1e-3f * powf(10, p_dbm / 10);   /* A */
    float v_on  = i_on * RF[range];
    float fh = 49.7f + 0.6f * urand(), ph = 2 * M_PI * urand();
    for (int n = 0; n < N; n++) {
        float t = n / FS;
        float light = modulated ? (fmodf(t * 2000.0f, 1.0f) < 0.5f ? v_on : 0) : v_on;
        float v = VREF + off + light
                + 2e-3f * sinf(2 * M_PI * fh * t + ph)
                + 0.5e-3f * sinf(2 * M_PI * 3 * fh * t + 3 * ph)
                + 150e-6f * gauss();
        float lsb = 3.0f / 65536;          /* 3.0 V ADC reference */
        if (v < 0) v = 0; if (v > 3.0f) v = 3.0f;
        x[n] = roundf(v / lsb) * lsb;
    }
}

int main(void)
{
    srand(12345);
    printf("Power   Range   DC method error (dB)    Tone method error (dB)\n");
    printf("(dBm)   (Rf)    mean |err|  95th pct    mean |err|  95th pct\n");
    for (int p = 0; p >= -80; p -= 5) {
        float v10 = responsivity(1310) * 1e-3f * powf(10, p / 10.0f) * RF[0];
        int r = pick_range(v10);
        double e_dc[TRIALS], e_t[TRIALS], s_dc = 0, s_t = 0;
        for (int k = 0; k < TRIALS; k++) {
            float off = (float)(2e-3 * urand() - 1e-3);
            make_block(p, 0, r, off);
            float vdc = dc_mean(x, N, VREF);         /* zero stored earlier */
            float d_dc = volts_to_dbm(vdc, r, 1310);
            make_block(p, 1, r, off);
            float a1 = goertzel_amp(x, N, 2000.0f, FS);
            float d_t = volts_to_dbm(tone_to_on_volts(a1), r, 1310);
            e_dc[k] = fabs(d_dc - p); e_t[k] = fabs(d_t - p);
            if (e_dc[k] > 40) e_dc[k] = 40;          /* negative reading */
            s_dc += e_dc[k]; s_t += e_t[k];
        }
        /* 95th percentile by sort */
        for (int i = 0; i < TRIALS; i++) for (int j = i + 1; j < TRIALS; j++) {
            if (e_dc[j] < e_dc[i]) { double t = e_dc[i]; e_dc[i] = e_dc[j]; e_dc[j] = t; }
            if (e_t[j]  < e_t[i])  { double t = e_t[i];  e_t[i]  = e_t[j];  e_t[j]  = t; }
        }
        int q = (int)(0.95 * TRIALS);
        const char *rn[] = {"10M", "1M", "100k", "10k", "1k"};
        printf("%5d   %-5s   %8.3f    %8.3f      %8.3f    %8.3f\n",
               p, rn[r], s_dc / TRIALS, e_dc[q], s_t / TRIALS, e_t[q]);
    }

    /* Auto-range hysteresis test: sweep power up and down in 0.1 dB steps,
     * count range changes. Each of the 4 boundaries should be crossed once
     * each way = 8 changes, with no chatter. */
    int r = 0, changes = 0;
    for (int dir = 0; dir < 2; dir++)
        for (int s = 0; s <= 800; s++) {
            float p = dir == 0 ? -75 + s * 0.1f : 5 - s * 0.1f;
            for (int rep = 0; rep < 3; rep++) {     /* 3 readings per step */
                float v = responsivity(1310) * 1e-3f * powf(10, p / 10) * RF[r];
                v += 150e-6f * (float)gauss();
                int nr = autorange(r, v);
                if (nr != r) { changes++; r = nr; }
            }
        }
    printf("\nAuto-range sweep -75 -> +5 -> -75 dBm: %d range changes "
           "(ideal 8, no chatter)\n", changes);

    /* Goertzel cost on the target */
    printf("Goertzel cost: %d multiply-adds per 100 ms block per tone\n", N);
    return 0;
}
