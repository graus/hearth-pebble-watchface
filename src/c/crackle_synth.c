#include "crackle_synth.h"

#include <string.h>

static uint32_t next(uint32_t *r) {
  uint32_t x = *r;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return *r = x;
}

// Uniform integer in [lo, hi].
static int range(uint32_t *r, int lo, int hi) {
  return lo + (int)(next(r) % (uint32_t)(hi - lo + 1));
}

// White noise in [-1024, 1024].
static int noise(uint32_t *r) {
  return (int)(next(r) >> 21) - 1024;
}

// One quarter of a sine wave, 0..1024, for the pop's thump.
static const int16_t s_quarter_sine[17] = {
  0, 100, 200, 297, 392, 483, 569, 650, 724, 792, 851, 903, 946, 980, 1004, 1019, 1024,
};

static int sine(uint32_t phase) {  // phase: 0..63 is one full turn
  uint32_t p = phase & 63;
  int q = (int)(p & 15);
  switch (p >> 4) {
    case 0: return s_quarter_sine[q];
    case 1: return s_quarter_sine[16 - q];
    case 2: return -s_quarter_sine[q];
    default: return -s_quarter_sine[16 - q];
  }
}

// The mix: each part adds at most about +-4096 per sample, so a cluster of
// up to seven parts fits in 16 bits (adds saturate just in case). It is then
// scaled to its loudest sample so every crackle uses the full 8 bits.
static int16_t s_mix[CRACKLE_MAX_SAMPLES];

static void mix(int i, int32_t v) {
  int32_t m = s_mix[i] + v;
  s_mix[i] = (int16_t)(m > 32767 ? 32767 : (m < -32768 ? -32768 : m));
}

// A snap: a one-sample spike, then noise that dies away within a few ms.
// `dull` smooths the noise for a softer, woodier crack.
static void add_snap(uint32_t *r, int at, int amp, int tau_samples, int dull) {
  int32_t level = amp << 10;  // amp in 0..1024, level in 1/1024ths
  // decay per sample: (1 - 1/tau) in 1/4096ths
  int32_t keep = 4096 - 4096 / (tau_samples > 1 ? tau_samples : 1);
  int prev = 0;
  int sign = (next(r) & 1) ? 1 : -1;
  mix(at, sign * amp * 4);  // the initial click
  for (int i = at + 1; i < CRACKLE_MAX_SAMPLES && level > 256; i++) {
    int n = noise(r);
    if (dull) {
      n = (n + prev) / 2;
      prev = n;
    }
    mix(i, (n * (level >> 10)) >> 8);
    level = (int32_t)(((int64_t)level * keep) >> 12);
  }
}

// A pop: a short, damped low thump (150-350 Hz) with a little noise on top.
static void add_pop(uint32_t *r, int at, int amp) {
  int freq = range(r, 150, 350);
  uint32_t step = (uint32_t)(freq * 64 * 256 / CRACKLE_RATE);  // 1/256ths of a table step
  uint32_t phase = 0;
  int32_t level = amp << 10;
  int32_t keep = 4096 - 4096 / range(r, 120, 220);  // ~8-14 ms
  for (int i = at; i < CRACKLE_MAX_SAMPLES && level > 256; i++) {
    mix(i, ((sine(phase >> 8) * (level >> 10)) >> 8) + ((noise(r) * (level >> 10)) >> 10));
    phase += step;
    level = (int32_t)(((int64_t)level * keep) >> 12);
  }
}

// The ember hiss: quiet noise fading over ~40 ms.
static void add_hiss(uint32_t *r, int at, int amp) {
  int32_t level = amp << 10;
  int32_t keep = 4096 - 4096 / 640;
  int prev = 0;
  for (int i = at; i < CRACKLE_MAX_SAMPLES && level > 256; i++) {
    int n = (noise(r) + prev) / 2;
    prev = n;
    mix(i, (n * (level >> 10)) >> 8);
    level = (int32_t)(((int64_t)level * keep) >> 12);
  }
}

uint16_t crackle_synth(int8_t *buf, uint32_t *rng) {
  memset(s_mix, 0, sizeof(s_mix));
  // 1 to 5 snaps, each later one usually quieter, 3-35 ms apart.
  int snaps = 1;
  while (snaps < 5 && (next(rng) & 3) != 0 && (snaps == 1 || (next(rng) & 1))) {
    snaps++;
  }
  int at = range(rng, 0, 48);
  int amp = range(rng, 700, 1024);
  int end = at;
  for (int i = 0; i < snaps && at < CRACKLE_MAX_SAMPLES - 400; i++) {
    add_snap(rng, at, amp, range(rng, 10, 48), (next(rng) & 3) == 0);
    end = at;
    at += range(rng, 48, 560);
    amp = amp * range(rng, 45, 100) / 100;
    if (amp < 200) amp = 200;
  }
  // A quarter of crackles carry the deeper pop of a sap pocket.
  if ((next(rng) & 3) == 0) {
    add_pop(rng, range(rng, 0, end > 0 ? end : 1), range(rng, 500, 900));
  }
  // A faint hiss after the last snap.
  add_hiss(rng, end, range(rng, 60, 140));

  // Length: up to where the sound has died away, plus a little.
  int32_t peak = 1;
  int len = 0;
  for (int i = 0; i < CRACKLE_MAX_SAMPLES; i++) {
    int32_t a = s_mix[i] < 0 ? -s_mix[i] : s_mix[i];
    if (a > peak) peak = a;
  }
  for (int i = 0; i < CRACKLE_MAX_SAMPLES; i++) {
    int32_t a = s_mix[i] < 0 ? -s_mix[i] : s_mix[i];
    if (a > peak / 64) len = i + 1;
  }
  if (len < 320) len = 320 < CRACKLE_MAX_SAMPLES ? 320 : CRACKLE_MAX_SAMPLES;
  for (int i = 0; i < len; i++) {
    buf[i] = (int8_t)((int32_t)s_mix[i] * 127 / peak);
  }
  return (uint16_t)len;
}
