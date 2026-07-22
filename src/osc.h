#ifndef OSCILLATOR_H
#define OSCILLATOR_H

#include <stdint.h>
#define UNISON 5


typedef enum {
    OSC_SINE,
    OSC_SQUARE,
    OSC_SAW,
    OSC_TRIANGLE,
    OSC_Custom
} OscWaveform;

typedef enum {
    ENV_Started,
    ENV_Releasing,
    ENV_Sustain,
    ENV_Idle
} EnvState;



typedef struct {
    float sample_rate_inv;
    uint32_t phase[UNISON];

    uint32_t precalc_dt[UNISON];
    OscWaveform waveform;


    int16_t amplitude;
    uint8_t attack;
    uint8_t release;

    EnvState envelope_state;
} Oscillator;


void oscillator_note_on(Oscillator* osc, float frequency, char key);
void oscillator_note_off(Oscillator* osc);
void oscillator_set_waveform(Oscillator* osc, OscWaveform waveform);
void oscillator_next(Oscillator* osc, int16_t* left, int16_t* right);
void oscillator_init_table(void);

#endif