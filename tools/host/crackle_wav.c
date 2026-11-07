// Renders Hearth's crackle to a WAV file, with the same synthesiser
// (src/c/crackle_synth.c) and the same timing as the watch, so the sound can
// be judged on a computer.
//
//   make -C tools/host crackle.wav   (or: ./crackle_wav out.wav [seconds] [seed])

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/c/crackle_synth.h"

static void put32(FILE *f, uint32_t v) { fputc(v, f); fputc(v >> 8, f); fputc(v >> 16, f); fputc(v >> 24, f); }
static void put16(FILE *f, uint16_t v) { fputc(v, f); fputc(v >> 8, f); }

int main(int argc, char **argv) {
  const char *path = argc > 1 ? argv[1] : "crackle.wav";
  int seconds = argc > 2 ? atoi(argv[2]) : 12;
  uint32_t rng = argc > 3 ? (uint32_t)atoi(argv[3]) : 2026;
  if (rng == 0) rng = 1;
  uint32_t total = (uint32_t)seconds * CRACKLE_RATE;
  int16_t *out = calloc(total, sizeof(int16_t));
  int8_t buf[CRACKLE_MAX_SAMPLES];
  if (!out) return 1;

  // The watch's timing (src/c/crackle.c): a third of the time a quick
  // follow-up after 80-200 ms, otherwise 400-2600 ms; loudness 80-127 of 127.
  uint32_t t = CRACKLE_RATE / 4;
  srand(rng);
  while (t < total) {
    uint16_t len = crackle_synth(buf, &rng);
    int velocity = 80 + rand() % 48;
    for (uint16_t i = 0; i < len && t + i < total; i++) {
      int32_t v = out[t + i] + buf[i] * 256 * velocity / 127;
      out[t + i] = (int16_t)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
    }
    uint32_t gap_ms = (rand() % 3 == 0) ? 80 + rand() % 121 : 400 + rand() % 2201;
    // The watch waits for the speaker to be idle before the next crackle.
    uint32_t gap = gap_ms * CRACKLE_RATE / 1000;
    t += gap > len ? gap : len;
  }

  FILE *f = fopen(path, "wb");
  if (!f) return 1;
  fputs("RIFF", f); put32(f, 36 + total * 2); fputs("WAVE", f);
  fputs("fmt ", f); put32(f, 16); put16(f, 1); put16(f, 1);
  put32(f, CRACKLE_RATE); put32(f, CRACKLE_RATE * 2); put16(f, 2); put16(f, 16);
  fputs("data", f); put32(f, total * 2);
  fwrite(out, sizeof(int16_t), total, f);
  fclose(f);
  printf("wrote %s: %d s at %d Hz\n", path, seconds, CRACKLE_RATE);
  free(out);
  return 0;
}
