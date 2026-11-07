#ifndef METER_DSP_H
#define METER_DSP_H

#define NRANGES 5
#define UP_V    2.80f   /* above this: switch to less gain  */
#define DOWN_V  0.25f   /* below this: switch to more gain  */

extern const float RF[NRANGES];

float responsivity(int lambda_nm);
float goertzel_amp(const float *x, int n, float f_hz, float fs);
float dc_mean(const float *x, int n, float zero_v);
float volts_to_dbm(float v, int range, int lambda_nm);
float tone_to_on_volts(float a1);
int   autorange(int range, float v_signal);

#endif
