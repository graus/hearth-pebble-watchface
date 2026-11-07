#include "fire.h"

#include <stdlib.h>
#include <string.h>

// Doom's offsets reach 4 cells sideways at most (randIdx 0..3 against the
// "+ 1" plus wind), so one wrap is enough for rows at least this wide.
#define MIN_WIDTH 8

struct Fire {
  uint16_t w;           // columns
  uint16_t h;           // visible rows; row h is the fuel
  uint8_t *cells;       // (FIRE_MAX_ROWS + 1) * w
  uint32_t rng;         // xorshift32 state, never 0
  int8_t offset;        // Doom's "+ 1", plus wind
  uint8_t top_heat;
};

// Doom picks the offset with Math.round(Math.random() * 3.0), which is
// not uniform: 0 and 3 come up half as often as 1 and 2. Six equal slots
// reproduce that exactly.
static const uint8_t s_rand_idx[6] = {0, 1, 1, 2, 2, 3};

static uint8_t *cell_row(const Fire *fire, uint16_t y) {
  return fire->cells + (uint32_t)y * fire->w;
}

// Rows per heat level lost, in 1/1024ths, for each offset (-1..4). Doom's
// rule decays half a level per row, but its column-by-column order lets
// heat written to the right of the current column spread again in the same
// frame, so the effective decay depends on the offset (Doom's own "+ 1"
// gives 0.43 levels per row). Measured on the host, any width.
static const uint16_t s_rows_per_level[6] = {2025, 2022, 2403, 1713, 2481, 2019};

static uint16_t rows_for(const Fire *fire) {
  // Heat stops at 0, which lifts the mean of a nearly dead top row by about
  // two levels; give the lowest tips a few more rows to burn out in.
  uint32_t loss = FIRE_MAX_HEAT - fire->top_heat + (fire->top_heat < 3 ? 3 - fire->top_heat : 0);
  uint32_t rows = (loss *
                   s_rows_per_level[fire->offset + 1] + 512) >> 10;
  return rows < 4 ? 4 : (rows > FIRE_MAX_ROWS ? FIRE_MAX_ROWS : rows);
}

// Resizes to the rows the tip heat and wind need. Rows added at the bottom
// continue the flames down to the fuel.
static void update_rows(Fire *fire) {
  uint16_t old_h = fire->h;
  fire->h = rows_for(fire);
  if (fire->h > old_h) {
    const uint8_t *last = fire->cells + (uint32_t)(old_h - 1) * fire->w;
    for (uint16_t y = old_h; y < fire->h; y++) {
      uint8_t *row = fire->cells + (uint32_t)y * fire->w;
      for (uint16_t x = 0; x < fire->w; x++) {
        row[x] = last[x] + (uint32_t)(FIRE_MAX_HEAT - last[x]) * (y - old_h + 1) / (fire->h - old_h + 1);
      }
    }
  }
  memset(fire->cells + (uint32_t)fire->h * fire->w, FIRE_MAX_HEAT, fire->w);
}

Fire *fire_create(uint16_t w, uint32_t seed) {
  if (w < MIN_WIDTH) {
    return NULL;
  }
  Fire *fire = malloc(sizeof(Fire));
  if (!fire) {
    return NULL;
  }
  memset(fire, 0, sizeof(Fire));
  fire->cells = malloc((uint32_t)(FIRE_MAX_ROWS + 1) * w);
  if (!fire->cells) {
    free(fire);
    return NULL;
  }
  fire->w = w;
  fire->rng = seed ? seed : 0x9E3779B9u;
  fire->offset = 1;
  fire->top_heat = 9;
  fire->h = rows_for(fire);
  // Start settled: an even gradient from the tip heat at the top to the fuel,
  // so the first frame already looks like fire.
  for (uint16_t y = 0; y <= fire->h; y++) {
    uint8_t heat = fire->top_heat + (uint32_t)(FIRE_MAX_HEAT - fire->top_heat) * y / fire->h;
    memset(cell_row(fire, y), heat, w);
  }
  return fire;
}

void fire_destroy(Fire *fire) {
  if (fire) {
    free(fire->cells);
    free(fire);
  }
}

// Doom's loop and spreadFire, as published by Fabien Sanglard in
// https://github.com/fabiensanglard/DoomFirePSX (flames.html):
//
//   function spreadFire(src) {
//     var pixel = firePixels[src];
//     if (pixel == 0) {
//       firePixels[src - FIRE_WIDTH] = 0;
//     } else {
//       var randIdx = Math.round(Math.random() * 3.0);// & 3;
//       var dst = src - randIdx + 1;
//       firePixels[dst - FIRE_WIDTH] = pixel - (randIdx & 1);
//     }
//   }
//   for (x = 0; x < FIRE_WIDTH; x++)
//     for (y = 1; y < FIRE_HEIGHT; y++)
//       spreadFire(y * FIRE_WIDTH + x);
//
// Same order (column by column), same rule, same distribution. Differences:
// xorshift32 instead of Math.random, the "+ 1" carries the wind, and a
// shifted write wraps within its row instead of into the next one.
void fire_step(Fire *fire) {
  const int w = fire->w;
  const uint16_t h = fire->h;
  const int offset = fire->offset;
  uint32_t r = fire->rng;
  for (int x = 0; x < w; x++) {
    for (uint16_t y = 1; y <= h; y++) {
      uint8_t *above = fire->cells + (uint32_t)(y - 1) * w;
      uint8_t pixel = above[w + x];
      if (pixel == 0) {
        above[x] = 0;
        continue;
      }
      r ^= r << 13;
      r ^= r >> 17;
      r ^= r << 5;
      int rand_idx = s_rand_idx[((r >> 16) * 6) >> 16];
      int dst = x - rand_idx + offset;
      if (dst < 0) {
        dst += w;
      } else if (dst >= w) {
        dst -= w;
      }
      above[dst] = pixel - (rand_idx & 1);
    }
  }
  fire->rng = r;
}

const uint8_t *fire_row(const Fire *fire, uint16_t y) {
  return cell_row(fire, y);
}

uint16_t fire_width(const Fire *fire) {
  return fire->w;
}

uint16_t fire_height(const Fire *fire) {
  return fire->h;
}

void fire_set_reach(Fire *fire, uint8_t top_heat) {
  fire->top_heat = top_heat > FIRE_MAX_REACH ? FIRE_MAX_REACH : top_heat;
  update_rows(fire);
}

void fire_set_wind(Fire *fire, int8_t wind) {
  if (wind > 3) wind = 3;
  if (wind < -2) wind = -2;
  if (fire->offset != 1 + wind) {
    fire->offset = 1 + wind;
    update_rows(fire);
  }
}

void fire_stoke(Fire *fire, uint8_t heat) {
  for (uint16_t y = 0; y < fire->h; y++) {
    uint8_t *row = cell_row(fire, y);
    for (uint16_t x = 0; x < fire->w; x++) {
      int v = row[x] + heat;
      row[x] = v > FIRE_MAX_HEAT ? FIRE_MAX_HEAT : v;
    }
  }
}
