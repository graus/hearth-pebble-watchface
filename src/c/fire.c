#include "fire.h"

#include <stdlib.h>
#include <string.h>

struct Fire {
  uint16_t w, h;        // visible cells; the grid has h + 1 rows (fuel last)
  uint16_t stride;      // w + 2 * FIRE_MAX_SHIFT
  uint8_t *cells;       // (h + 1) * stride
  uint32_t rng;         // xorshift32 state, never 0
  int8_t turbulence[4];
  int8_t wind;
  uint8_t top_heat;
  uint8_t fuel;
  int8_t shifts[4];     // turbulence + wind, what fire_step uses
  uint8_t cool_quantum; // heat lost when a cell cools...
  uint16_t cool_chance; // ...which it does with this chance in 1024ths
};

static const int8_t s_default_turbulence[4] = {-1, 0, 0, 1};

static uint8_t *cell_row(const Fire *fire, uint16_t y) {
  return fire->cells + (uint32_t)y * fire->stride + FIRE_MAX_SHIFT;
}

// Cooling is a chance per step, chosen so that a cell rising from the fuel row
// loses (FIRE_MAX_HEAT - reach) heat on average by the time it reaches the top,
// whatever the grid height. The chance is in 1024ths, not 64ths: on a 178-row
// grid 64ths truncate 12.9 to 12 and the flames overshoot the top by 2-3 levels.
//
// Short grids (B&W, 4 px cells) need more than one level per step. Losing a
// fixed level every step would make a smooth gradient with no flames in it,
// so a cell instead loses a larger quantum with a chance below one: the same
// mean, but the randomness that makes tongues survives.
static void update_cooling(Fire *fire) {
  int reach = fire->top_heat + fire->fuel;
  if (reach > FIRE_MAX_HEAT) {
    reach = FIRE_MAX_HEAT;
  }
  // Heat clamps at 0, which lifts the mean of a nearly dead top row by about
  // two levels; aim lower at the bottom of the range to compensate.
  uint32_t loss = FIRE_MAX_HEAT - reach + (reach < 2 ? 2 - reach : 0);
  uint32_t chance = (loss * 1024 + fire->h / 2) / fire->h;
  uint32_t quantum = (chance + 1023) >> 10;
  if (quantum == 0) quantum = 1;
  fire->cool_quantum = quantum;
  fire->cool_chance = (chance + quantum / 2) / quantum;
}

// A column that no source can reach (upwind edge under strong wind) would
// otherwise keep a stale value forever. Clear every such column; it stays
// clear because nothing writes to it.
static void update_shifts(Fire *fire) {
  int min = FIRE_MAX_SHIFT, max = -FIRE_MAX_SHIFT;
  for (int i = 0; i < 4; i++) {
    int s = fire->turbulence[i] + fire->wind;
    if (s > FIRE_MAX_SHIFT) s = FIRE_MAX_SHIFT;
    if (s < -FIRE_MAX_SHIFT) s = -FIRE_MAX_SHIFT;
    fire->shifts[i] = (int8_t)s;
    if (s < min) min = s;
    if (s > max) max = s;
  }
  for (uint16_t y = 0; y < fire->h; y++) {
    uint8_t *row = cell_row(fire, y);
    for (int x = 0; x < min && x < fire->w; x++) {
      row[x] = 0;
    }
    for (int x = fire->w + max; x < fire->w; x++) {
      if (x >= 0) row[x] = 0;
    }
  }
}

Fire *fire_create(uint16_t w, uint16_t h, uint32_t seed) {
  if (w == 0 || h == 0) {
    return NULL;
  }
  Fire *fire = malloc(sizeof(Fire));
  if (!fire) {
    return NULL;
  }
  memset(fire, 0, sizeof(Fire));
  fire->w = w;
  fire->h = h;
  fire->stride = w + 2 * FIRE_MAX_SHIFT;
  fire->cells = malloc((uint32_t)(h + 1) * fire->stride);
  if (!fire->cells) {
    free(fire);
    return NULL;
  }
  fire->rng = seed ? seed : 0x9E3779B9u;
  fire->top_heat = 9;
  memcpy(fire->turbulence, s_default_turbulence, sizeof(fire->turbulence));
  update_cooling(fire);

  // Start settled: an even gradient from the reach at the top to full heat at
  // the bottom, so the first frame already looks like fire.
  for (uint16_t y = 0; y <= h; y++) {
    uint8_t heat = fire->top_heat + (uint32_t)(FIRE_MAX_HEAT - fire->top_heat) * y / h;
    memset(fire->cells + (uint32_t)y * fire->stride, heat, fire->stride);
  }
  update_shifts(fire);
  return fire;
}

void fire_destroy(Fire *fire) {
  if (fire) {
    free(fire->cells);
    free(fire);
  }
}

// Per cell: one xorshift32, a table lookup and a few ALU ops. Rows are visited
// top to bottom, and each row writes into the row above, which has already
// been read this frame, so one grid suffices.
void fire_step(Fire *fire) {
  const uint16_t w = fire->w;
  const uint16_t stride = fire->stride;
  const int cool_quantum = fire->cool_quantum;
  const uint32_t cool_chance = fire->cool_chance;
  const int8_t *shifts = fire->shifts;
  uint32_t r = fire->rng;
  for (uint16_t y = 1; y <= fire->h; y++) {
    const uint8_t *src = cell_row(fire, y);
    uint8_t *dst = (uint8_t *)src - stride;
    for (uint16_t x = 0; x < w; x++) {
      r ^= r << 13;
      r ^= r >> 17;
      r ^= r << 5;
      // High bits are the better-mixed ones: 16-17 shift, 20-29 cooling.
      int heat = src[x] - (((r >> 20) & 1023) < cool_chance ? cool_quantum : 0);
      dst[x + shifts[(r >> 16) & 3]] = heat > 0 ? heat : 0;
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
  fire->top_heat = top_heat > FIRE_MAX_HEAT ? FIRE_MAX_HEAT : top_heat;
  update_cooling(fire);
}

void fire_set_wind(Fire *fire, int8_t wind) {
  if (wind > 2) wind = 2;
  if (wind < -2) wind = -2;
  if (wind != fire->wind) {
    fire->wind = wind;
    update_shifts(fire);
  }
}

void fire_set_turbulence(Fire *fire, const int8_t shifts[4]) {
  for (int i = 0; i < 4; i++) {
    int s = shifts[i];
    fire->turbulence[i] = s > 2 ? 2 : (s < -2 ? -2 : s);
  }
  update_shifts(fire);
}

void fire_set_fuel(Fire *fire, uint8_t fuel) {
  fire->fuel = fuel > FIRE_MAX_HEAT ? FIRE_MAX_HEAT : fuel;
  update_cooling(fire);
}
