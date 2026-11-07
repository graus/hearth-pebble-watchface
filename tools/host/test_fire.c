// Host tests for the fire engine. Build and run with `make -C tools/host`.
// Built with AddressSanitizer and UBSan, so any write outside the grid fails.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/c/fire.h"

// Fire widths per platform: (worst-case pair width + column gap), from
// src/c/generated/metrics.h, in cells of 1, 2 and 4 px (B&W: 4 px only,
// window rounded out to whole bytes).
typedef struct {
  const char *name;
  int window_w;
  int bw;
} Platform;

// Each platform has a large digit set and a compact one (for Timeline Quick
// View); both windows are tested.
static const Platform s_platforms[] = {
  {"aplite/diorite/flint large", 128 + 7, 1},
  {"aplite/diorite/flint compact", 78 + 8, 1},
  {"basalt large", 128 + 7, 0},
  {"basalt/chalk compact", 78 + 8, 0},
  {"chalk large", 108 + 9, 0},
  {"emery large", 182 + 10, 0},
  {"emery compact", 154 + 16, 0},
  {"gabbro large", 158 + 13, 0},
  {"gabbro compact", 144 + 15, 0},
};

static int s_failures;

#define CHECK(cond, ...) do { \
  if (!(cond)) { s_failures++; printf("FAIL: " __VA_ARGS__); printf("\n"); } \
} while (0)

static int check_range(const Fire *fire) {
  for (int y = 0; y < fire_height(fire); y++) {
    const uint8_t *r = fire_row(fire, y);
    for (int x = 0; x < fire_width(fire); x++) {
      if (r[x] > FIRE_MAX_HEAT) {
        CHECK(0, "heat %d out of range at %d,%d", r[x], x, y);
        return 0;
      }
    }
  }
  return 1;
}

// Mean heat of the top row over many frames, after the fire settles.
static double top_mean(Fire *fire, int warmup, int frames) {
  uint16_t w = fire_width(fire);
  for (int i = 0; i < warmup; i++) fire_step(fire);
  double sum = 0;
  for (int f = 0; f < frames; f++) {
    fire_step(fire);
    const uint8_t *row = fire_row(fire, 0);
    for (int x = 0; x < w; x++) sum += row[x];
    if (f % 50 == 0 && !check_range(fire)) return -1;
  }
  return sum / ((double)frames * w);
}

static double grid_mean(const Fire *fire) {
  double sum = 0;
  for (int y = 0; y < fire_height(fire); y++) {
    const uint8_t *r = fire_row(fire, y);
    for (int x = 0; x < fire_width(fire); x++) sum += r[x];
  }
  return sum / ((double)fire_width(fire) * fire_height(fire));
}

int main(void) {
  int cases = 0;
  for (size_t p = 0; p < sizeof(s_platforms) / sizeof(s_platforms[0]); p++) {
    const Platform *pl = &s_platforms[p];
    for (int shift = 0; shift <= 2; shift++) {
      if (pl->bw && shift != 2) continue;
      int cell = 1 << shift;
      int window_w = pl->bw ? ((pl->window_w + 7 + 7) & ~7) : pl->window_w;
      int w = (window_w + cell - 1) / cell;
      // Wind: -2..2 (setting, weather or tilt; the sum is capped).
      for (int wind = -2; wind <= 2; wind++) {
        if (pl->bw && wind != 0) continue;  // B&W has no wind settings
        static const int reaches[] = {0, 1, 3, 6, 9, 12, 16, 20};
        for (size_t ri = 0; ri < sizeof(reaches) / sizeof(reaches[0]); ri++) {
          int reach = reaches[ri];
          Fire *fire = fire_create(w, 12345 + reach);
          CHECK(fire != NULL, "create %d", w);
          if (!fire) continue;
          fire_set_wind(fire, wind);
          fire_set_reach(fire, reach);
          CHECK(fire_height(fire) <= FIRE_MAX_ROWS, "rows %d", fire_height(fire));
          // Narrow fires have few top cells, so average over more frames
          // (about 200,000 samples) to keep the noise well under 1.
          int frames = 200000 / w;
          double mean = top_mean(fire, 3 * fire_height(fire) + 50, frames);
          CHECK(mean >= reach - 2 && mean <= reach + 2,
                "%s %d px (%d cells) wind %d reach %d: top mean %.2f",
                pl->name, cell, w, wind, reach, mean);
          // Stoke: an immediate leap, measured over the whole grid.
          double before = grid_mean(fire);
          fire_stoke(fire, 8);
          double stoked = grid_mean(fire);
          CHECK(stoked > before + 3, "%s %d px wind %d reach %d: stoke did not raise the fire (%.2f vs %.2f)",
                pl->name, cell, wind, reach, stoked, before);
          check_range(fire);
          fire_destroy(fire);
          cases++;
        }
      }
    }
  }

  // Changing reach and wind while running (weather, sun, tilt) resizes the
  // rows; it must stay in bounds and in range.
  Fire *fire = fire_create(81, 7);
  CHECK(fire != NULL, "create 81");
  if (fire) {
    srand(3);
    for (int i = 0; i < 3000; i++) {
      if (i % 7 == 0) fire_set_reach(fire, rand() % (FIRE_MAX_REACH + 5));
      if (i % 5 == 0) fire_set_wind(fire, rand() % 9 - 4);
      if (i % 97 == 0) fire_stoke(fire, rand() % 40);
      fire_step(fire);
      if (i % 100 == 0) check_range(fire);
    }
    fire_destroy(fire);
    cases++;
  }

  // Degenerate sizes must not crash.
  CHECK(fire_create(0, 1) == NULL, "zero width");
  CHECK(fire_create(7, 1) == NULL, "too narrow to wrap once");
  Fire *narrow = fire_create(8, 0);
  CHECK(narrow != NULL, "8 wide");
  if (narrow) {
    fire_set_wind(narrow, 3);
    fire_set_reach(narrow, 0);
    for (int i = 0; i < 500; i++) fire_step(narrow);
    fire_set_wind(narrow, -2);
    for (int i = 0; i < 500; i++) fire_step(narrow);
    check_range(narrow);
    fire_destroy(narrow);
  }
  printf("%d cases, %d failures\n", cases, s_failures);
  return s_failures ? 1 : 0;
}
