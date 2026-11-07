/* meter_dsp.c - FiberCheck optical power meter DSP (portable C, runs on
 * STM32G474 with -DON_TARGET, or on a PC for testing).
 *
 *  - Goertzel detector for the 270 Hz / 1 kHz / 2 kHz identification tones
 *  - Auto-ranging across 5 transimpedance gains, with hysteresis
 *  - Conversion from TIA voltage to optical power in dBm
 */
#include <math.h>
#include <stdint.h>
#include "meter_dsp.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Feedback resistors of the 5 TIA ranges, ohms (range 0 = most sensitive) */
const float RF[NRANGES] = {10e6f, 1e6f, 100e3f, 10e3f, 1e3f};

/* InGaAs responsivity (A/W) by wavelength; datasheet-typical values,
 * replaced by per-unit calibration in production */
float responsivity(int lambda_nm)
{
    switch (lambda_nm) {
    case 1310: return 0.90f;
    case 1490: return 0.95f;
    case 1550: return 0.95f;
    default:   return 0.90f;
    }
}

/* Goertzel: amplitude of the component at f_hz in x[0..n-1].
 * Cost: one multiply and two adds per sample. For an exact result choose
 * n so that n * f_hz / fs is an integer (tone falls on a bin centre). */
float goertzel_amp(const float *x, int n, float f_hz, float fs)
{
    float k = roundf(n * f_hz / fs);
    float w = 2.0f * (float)M_PI * k / n;
    float c = 2.0f * cosf(w);
    float s1 = 0, s2 = 0;
    for (int i = 0; i < n; i++) {
        float s0 = x[i] + c * s1 - s2;
        s2 = s1; s1 = s0;
    }
    float re = s1 - s2 * cosf(w);
    float im = s2 * sinf(w);
    return 2.0f * sqrtf(re * re + im * im) / n;   /* peak amplitude */
}

/* Plain DC method: mean of the block minus a stored zero reading */
float dc_mean(const float *x, int n, float zero_v)
{
    double acc = 0;
    for (int i = 0; i < n; i++) acc += x[i];
    return (float)(acc / n) - zero_v;
}

/* Optical power (dBm) from the TIA voltage that the light produced */
float volts_to_dbm(float v, int range, int lambda_nm)
{
    float i_pd = v / RF[range];                     /* photocurrent, A */
    float p_w  = i_pd / responsivity(lambda_nm);    /* optical power, W */
    return p_w > 0 ? 10.0f * log10f(p_w / 1e-3f) : -99.0f;
}

/* A 50 % on/off square wave between 0 and P has fundamental amplitude
 * (2/pi) * P, so the on-level (CW-equivalent) voltage is (pi/2) * A1 */
float tone_to_on_volts(float a1) { return a1 * (float)M_PI / 2.0f; }

/* Auto-range with hysteresis. Ranges are one decade apart, so a reading of
 * UP_V on range r reads UP_V/10 on range r+1. Because DOWN_V < UP_V/10 the
 * meter can never bounce between two ranges at a fixed power. */
int autorange(int range, float v_signal)
{
    if (v_signal > UP_V && range < NRANGES - 1) return range + 1; /* less gain */
    if (v_signal < DOWN_V && range > 0)         return range - 1; /* more gain */
    return range;
}
