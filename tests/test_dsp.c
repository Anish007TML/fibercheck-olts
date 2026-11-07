/* test_dsp.c - Unit tests for the FiberCheck firmware DSP.
 * Run: make test   (exits non-zero if any test fails)
 */
#include <stdio.h>
#include <math.h>
#include "meter_dsp.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define FS 32000.0f
#define N  3200

static int fails = 0;
static void check(int ok, const char *name, double got)
{
    printf("%s  %-52s (%.6g)\n", ok ? "PASS" : "FAIL", name, got);
    if (!ok) fails++;
}

static float x[N];

int main(void)
{
    /* 1. Goertzel recovers the amplitude of a pure 2 kHz sine */
    for (int n = 0; n < N; n++) x[n] = 1.0f * sinf(2 * M_PI * 2000 * n / FS);
    float a = goertzel_amp(x, N, 2000, FS);
    check(fabsf(a - 1.0f) < 1e-3f, "Goertzel: 1.0 V sine at 2 kHz reads 1.0", a);

    /* 2. Goertzel ignores DC offset and 50 Hz mains on the tone bin */
    for (int n = 0; n < N; n++) x[n] = 1.0f + 1.0f * sinf(2 * M_PI * 50 * n / FS);
    a = goertzel_amp(x, N, 2000, FS);
    check(a < 1e-3f, "Goertzel: DC + 50 Hz give ~0 at 2 kHz", a);

    /* 3. Goertzel separates the three identification tones */
    for (int n = 0; n < N; n++)
        x[n] = 0.5f * sinf(2 * M_PI * 270 * n / FS) + 0.2f * sinf(2 * M_PI * 1000 * n / FS);
    float a270 = goertzel_amp(x, N, 270, FS), a1k = goertzel_amp(x, N, 1000, FS),
          a2k = goertzel_amp(x, N, 2000, FS);
    check(fabsf(a270 - 0.5f) < 1e-3f && fabsf(a1k - 0.2f) < 1e-3f && a2k < 1e-3f,
          "Goertzel: 270 Hz / 1 kHz / 2 kHz kept separate", a270);

    /* 4. Square-wave fundamental -> on-level: (pi/2) * (2/pi) = 1 */
    float v = tone_to_on_volts(2.0f / M_PI);
    check(fabsf(v - 1.0f) < 1e-6f, "Tone: fundamental (2/pi) maps to on-level 1.0", v);

    /* 5. 0.9 V on the 1 kohm range at 1310 nm (0.9 A/W) = 1 mW = 0 dBm */
    float d = volts_to_dbm(0.9f, 4, 1310);
    check(fabsf(d) < 1e-3f, "dBm: 0.9 mA at 0.9 A/W = 0 dBm", d);

    /* 6. 0.9 V on the 10 Mohm range = 90 nA = 100 nW = -40 dBm */
    d = volts_to_dbm(0.9f, 0, 1310);
    check(fabsf(d + 40.0f) < 1e-3f, "dBm: 90 nA at 0.9 A/W = -40 dBm", d);

    /* 7. Auto-range: over-range goes to less gain, under-range to more */
    check(autorange(0, 2.9f) == 1 && autorange(2, 0.2f) == 1,
          "Auto-range: up above 2.8 V, down below 0.25 V", 0);

    /* 8. Hysteresis: 2.85 V on range r reads 0.285 V on r+1 and stays */
    int r = autorange(1, 2.85f);
    check(r == 2 && autorange(r, 0.285f) == 2, "Auto-range: no bounce after a switch", r);

    /* 9. Range limits are respected */
    check(autorange(0, 0.0f) == 0 && autorange(NRANGES - 1, 3.0f) == NRANGES - 1,
          "Auto-range: stays inside range 0..4", 0);

    printf("------------------------------------------------------------\n");
    printf(fails ? "%d TEST(S) FAILED\n" : "ALL %d TESTS PASSED\n", fails ? fails : 9);
    return fails ? 1 : 0;
}
