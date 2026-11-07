#pragma once
// Synthesises one fire crackle: a random cluster of sharp snaps, sometimes
// with the duller pop of a bursting sap pocket, and a faint ember hiss.
// Plain C with no Pebble dependency, so the same code renders a WAV on a
// computer (tools/host/crackle_wav.c) as plays on the watch.

#include <stdint.h>

#define CRACKLE_RATE 16000           // samples per second
#define CRACKLE_MAX_SAMPLES 2880     // 180 ms

// Writes one crackle as signed 8-bit samples into buf (at least
// CRACKLE_MAX_SAMPLES long) and returns how many samples it used.
// rng: xorshift32 state, never 0; advanced by the call.
uint16_t crackle_synth(int8_t *buf, uint32_t *rng);
