#ifndef FILTER_H
#define FILTER_H

typedef struct
{
    float s1[2];  // integrator 1 state (BP)
    float s2[2];  // integrator 2 state (LP)
    float g;       // tan(pi * fc / fs)
    float R;       // damping: R = 1/(2*Q)
    float D;       // precomputed denominator
} TPTSVF_STEREO;


void svf_stereo_reset(TPTSVF_STEREO* f);
void svf_stereo_set(TPTSVF_STEREO* f, float cutoff, float resonance, float sample_rate);
void svf_stereo_process(TPTSVF_STEREO* f, float* x, float* lp, float* bp, float* hp);

#endif