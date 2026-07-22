# microcontroller-synth
nrf5340 + zephyr + i2s audio

This is a simple software synthesizer running on the nRF5340 DK with Zephyr that generates audio in real time and streams it over I2S to a MAX98357A amplifier. The design is built around a completely fixed-point audio path for performance, using a 32-bit phase accumulator instead of calculating waveforms with floating-point math every sample.  Each voice uses multiple slightly detuned oscillators for a wider sound, and uses an attack+release evelope.
