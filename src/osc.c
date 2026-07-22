#include "osc.h"
#include <math.h>
#include <stdlib.h>
#define TWO_PI 6.28318530718f


//#define WAVE_TABLE


#define TRUE 1
#define FALSE 0


#ifdef WAVE_TABLE
#define TABLE_SIZE 2048

static float wavetable[TABLE_SIZE];
#endif
void oscillator_init_table(void)
{
#ifdef WAVE_TABLE
    for (int i = 0; i < TABLE_SIZE; i++)
    {
        float phase = (float)i / (float)TABLE_SIZE;

        // saw
        wavetable[i] = (2.0f * phase) - 1.0f;

        // or sine:
        // wavetable[i] = sinf(TWO_PI * phase);
    }
#endif
}

static inline float poly_blep(float t, float dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    else if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }

    return 0.0f;
}
static inline float clampf(float x, float min, float max)
{
    return fminf(fmaxf(x, min), max);
}


void oscillator_note_on(Oscillator *osc, float frequency, char key)
{
    osc->envelope_state = ENV_Started;

    static const float detune_ratio_table[7] =
    {
        0.9715f,
        0.9871f,
        0.9948f,
        1.0f,
        1.0052f,
        1.0131f,
        1.0293f
    };

    for (int i = 0; i < UNISON; i++)
    {
        osc->phase[i] = (uint32_t)((float)rand() / (float)RAND_MAX * 4294967296.0f);
        //osc->lfo[i] = ((float)rand() / (float)RAND_MAX) * TWO_PI;
        //osc->precalc_dt[i] = frequency * detune_ratio_table[i] * osc->sample_rate_inv;

        
        osc->precalc_dt[i] = (uint32_t)(frequency * detune_ratio_table[i] * osc->sample_rate_inv * 4294967296.0f);
    }
}

void oscillator_note_off(Oscillator *osc)
{
    osc->envelope_state = ENV_Releasing;
}

void oscillator_set_waveform(Oscillator* osc, OscWaveform waveform)
{
    osc->waveform = waveform;
}

void oscillator_next(Oscillator* osc, int16_t* left, int16_t* right)
{

    if (osc->envelope_state == ENV_Idle)
    {
        return;
    }

    switch (osc->envelope_state)
    {
        case ENV_Started:
            osc->amplitude += osc->attack;

            if (osc->amplitude > 255)
            {
                osc->amplitude = 255;
                osc->envelope_state = ENV_Sustain;
            }
            break;
        case ENV_Releasing:
            osc->amplitude -= osc->attack;
            
            if (osc->amplitude < 0)
            {
                osc->amplitude = 0;
                osc->envelope_state = ENV_Idle;
            }
            break;

        default:
            break;
    }

    int16_t sample = 0;
    uint32_t phase;
    for (int i = 0; i < UNISON; i++)
    {
        uint32_t ph = osc->phase[i];
        int32_t saw = (int32_t)(ph >> 16) - 32768;
        sample += (int16_t)((saw) >> 8);

        osc->phase[i] = ph + osc->precalc_dt[i];
    }
    *left  += sample*2;
    *right += sample*2;
}