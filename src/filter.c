#include "filter.h"
#include <math.h>
#define PI 3.14159265359f

void svf_stereo_reset(TPTSVF_STEREO* f)
{
    f->s1[0] = 0.0F;
    f->s1[1] = 0.0F;
    f->s2[0] = 0.0F;
    f->s2[1] = 0.0F;
}

void svf_stereo_set(TPTSVF_STEREO* f, float cutoff, float resonance, float sample_rate)
{
    f->g = tanf(PI * cutoff / sample_rate);
    f->R = 1.0f / (2.0f * resonance);
    f->D = 1.0f / (1.0f + 2.0f * f->R * f->g + f->g * f->g);
}

void svf_stereo_process(TPTSVF_STEREO* f, float* x, float* lp, float* bp, float* hp) 
{
    for (int i = 0; i < 2; i++)
    {
        // Solve the algebraic loop
        hp[i] = (x[i] - (2.0f * f->R + f->g) * f->s1[i] - f->s2[i]) * f->D;

        // Compute outputs from integrators
        float v1 = f->g * hp[i];
        bp[i] = v1 + f->s1[i];
        float v2 = f->g * bp[i];
        lp[i] = v2 + f->s2[i];

        // Update states (equiv: s = 2*out - s)
        f->s1[i] = bp[i] + v1;
        f->s2[i] = lp[i] + v2;
    }

}
