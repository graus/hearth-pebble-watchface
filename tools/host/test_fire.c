// Host tests for the fire engine. Build and run with `make -C tools/host`.
// Built with AddressSanitizer and UBSan, so any write outside the grid fails.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/c/fire.h"

// Flame window per platform: (worst-case pair width + column gap) x
// (two digit heights + row gap), from src/c/generated/metrics.h.
typedef struct {
  const char *name;
  int w, h;
  int bw;
} Platform;

static const Platform s_platforms[] = {
  {"aplite/diorite/flint", 78 + 8, 43 * 2 + 14, 1},
  {"basalt", 78 + 8, 43 * 2 + 14, 0},
  {"chalk", 78 + 8, 43 * 2 + 14, 0},
  {"emery", 146 + 16, 81 * 2 + 28, 0},
  {"gabbro", 138 + 15, 76 * 2 + 26, 0},
};

static const int8_t s_turbulence[3][4] = {
  {0, 0, 0, 0}, {-1, 0, 0, 1}, {-2, -1, 1, 2},
};

static int s_failures;

#define CHECK(cond, ...) do { \
  if (!(cond)) { s_failures++; printf("FAIL: " __VA_ARGS__); printf("\n"); } \
} while (0)

// Mean heat of the top row over many frames, after the fire settles.
static double top_mean(Fire *fire, int frames) {
  uint16_t w = fire_width(fire), h = fire_height(fire);
  for (int i = 0; i < 200; i++) fire_step(fire);
  double sum = 0;
  for (int f = 0; f < frames; f++) {
    fire_step(fire);
    const uint8_t *row = fire_row(fire, 0);
    for (int x = 0; x < w; x++) sum += row[x];
    for (int y = 0; y < h; y++) {
      const uint8_t *r = fire_row(fire, y);
      for (int x = 0; x < w; x++) {
        CHECK(r[x] <= FIRE_MAX_HEAT, "heat %d out of range at %d,%d", r[x], x, y);
        if (r[x] > FIRE_MAX_HEAT) return -1;
      }
    }
  }
  return sum / ((double)frames * w);
}

int main(void) {
  int cases = 0;
  for (size_t p = 0; p < sizeof(s_platforms) / sizeof(s_platforms[0]); p++) {
    const Platform *pl = &s_platforms[p];
    for (int shift = 0; shift <= 2; shift++) {
      int cell = 1 << shift;
      if (pl->bw && cell != 4) continue;
      // B&W rounds the window out to whole bytes on both sides.
      int win_w = pl->bw ? ((pl->w + 7 + 7) & ~7) : pl->w;
      int w = (win_w + cell - 1) / cell;
      int h = (pl->h + cell - 1) / cell;
      for (int t = 0; t < 3; t++) {
        for (int wind = -2; wind <= 2; wind++) {
          if (pl->bw && (t != 1 || wind != 0)) continue;  // B&W has no wind or turbulence settings
          for (int reach = 0; reach <= 20; reach += 1) {
            Fire *fire = fire_create(w, h, 12345 + reach);
            CHECK(fire != NULL, "create %dx%d", w, h);
            if (!fire) continue;
            fire_set_turbulence(fire, s_turbulence[t]);
            fire_set_wind(fire, wind);
            fire_set_reach(fire, reach);
            double mean = top_mean(fire, 100);
            // Wind blows the upwind columns out, so judge the mean only without it.
            if (wind == 0) {
              CHECK(mean >= reach - 2 && mean <= reach + 2,
                    "%s cell %d grid %dx%d turb %d reach %d: top mean %.2f",
                    pl->name, cell, w, h, t, reach, mean);
            }
            // Flare: fuel raises the top. Under wind the top row is fed by
            // chains that drifted in from the side, so judge only without it.
            fire_set_fuel(fire, 12);
            double flared = top_mean(fire, 20);
            if (wind == 0) CHECK(flared >= mean, "%s cell %d turb %d wind %d reach %d: flare did not raise the top (%.2f < %.2f)", pl->name, cell, t, wind, reach, flared, mean);
            fire_destroy(fire);
            cases++;
          }
        }
      }
    }
  }
  // Tiny and degenerate grids must not crash.
  CHECK(fire_create(0, 10, 1) == NULL, "zero width");
  CHECK(fire_create(10, 0, 1) == NULL, "zero height");
  Fire *one = fire_create(1, 1, 0);
  CHECK(one != NULL, "1x1");
  if (one) {
    static const int8_t wild[4] = {-2, -1, 1, 2};
    fire_set_turbulence(one, wild);
    fire_set_wind(one, 2);
    for (int i = 0; i < 100; i++) fire_step(one);
    fire_destroy(one);
  }
  printf("%d cases, %d failures\n", cases, s_failures);
  return s_failures ? 1 : 0;
}
