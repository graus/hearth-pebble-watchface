#include "fire.h"

#include <stdlib.h>
#include <string.h>

struct Fire {
  uint16_t w, h;        // visible cells; the grid has h + 1 rows (fuel last)
  uint16_t stride;      // w + 2 * FIRE_MAX_SHIFT
  uint8_t *cells;       // (h + 1) * stride
  uint32_t *drift;      // w entries: this row's drifting writes, applied last
  uint8_t *straight;    // one bit per cell of the stride: got straight-up heat
  uint32_t rng;         // xorshift32 state, never 0
  int8_t turbulence[4];
  int8_t wind;
  uint8_t top_heat;
  uint8_t fuel;
  int8_t shifts[4];     // turbulence + wind, what fire_step uses
  uint8_t cool_quantum; // heat lost when a cell cools...
  uint16_t cool_chance; // ...which it does with this chance in 1024ths
  uint16_t gain;        // cooling multiplier from calibration, 8.8 fixed point
  uint16_t frames;      // frames summed into top_sum
  uint32_t top_sum;     // heat of the top row over those frames
  uint16_t hold;        // frames to skip calibration after a stoke
};

static const int8_t s_default_turbulence[4] = {-1, 0, 0, 1};

// Calibration. Drifting heat loses every collision with heat rising straight
// up, and cells it leaves cool their old value (see fire_step), so the
// nominal cooling is off by an amount that depends on turbulence and grid
// height. Rather
// than a fragile formula, the engine measures the top row and corrects a
// cooling gain once per transport time (h frames, the delay before a change
// reaches the top).
#define GAIN_ONE 256
#define GAIN_MIN (GAIN_ONE / 4)
#define GAIN_MAX (4 * GAIN_ONE)
#define GAIN_K 6  // gain change per 1/16 level of error, in 1/16ths

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
  uint32_t nominal = (loss * 1024 + fire->h / 2) / fire->h;
  uint32_t chance = (nominal * fire->gain + GAIN_ONE / 2) / GAIN_ONE;
  uint32_t quantum = (chance + 1023) >> 10;
  if (quantum == 0) quantum = 1;
  fire->cool_quantum = quantum;
  fire->cool_chance = (chance + quantum / 2) / quantum;
}

// A column that no source can reach (upwind edge under strong wind) would
// otherwise keep a stale value forever. Clear every such column; it stays
// clear because nothing writes to it.
static void update_shifts(Fire *fire) {
  // Heat that does not cool rises straight up, so 0 is a shift too.
  int min = 0, max = 0;
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
  fire->drift = malloc((uint32_t)w * sizeof(uint32_t));
  fire->straight = malloc((fire->stride + 7) / 8);
  if (!fire->cells || !fire->drift || !fire->straight) {
    free(fire->cells);
    free(fire->drift);
    free(fire->straight);
    free(fire);
    return NULL;
  }
  fire->rng = seed ? seed : 0x9E3779B9u;
  fire->top_heat = 9;
  fire->gain = GAIN_ONE * 12 / 14;  // about right for the default turbulence
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
    free(fire->drift);
    free(fire->straight);
    free(fire);
  }
}

// Sums the top row each frame and, once per transport time, nudges the
// cooling gain toward the requested reach. Paused during a flare, whose
// extra fuel is meant to overshoot. Costs one add per top-row cell.
static void calibrate(Fire *fire) {
  if (fire->hold) {
    fire->hold--;
  }
  if (fire->fuel || fire->hold) {
    fire->frames = 0;
    fire->top_sum = 0;
    return;
  }
  const uint8_t *top = cell_row(fire, 0);
  uint32_t sum = 0;
  for (uint16_t x = 0; x < fire->w; x++) {
    sum += top[x];
  }
  fire->top_sum += sum;
  if (++fire->frames < fire->h + 8) {
    return;
  }
  int32_t mean16 = (int32_t)(fire->top_sum * 16 / ((uint32_t)fire->w * fire->frames));
  int32_t error16 = mean16 - fire->top_heat * 16;  // positive: too hot, cool more
  int32_t gain = fire->gain + error16 * GAIN_K / 16;
  fire->gain = gain < GAIN_MIN ? GAIN_MIN : (gain > GAIN_MAX ? GAIN_MAX : gain);
  fire->frames = 0;
  fire->top_sum = 0;
  update_cooling(fire);
}

// Per cell: one xorshift32, a table lookup and a few ALU ops; no function
// calls and no bounds checks. Rows are visited top to bottom, and each row
// writes into the row above, which has already been read this frame, so one
// grid suffices.
//
// Scatter (each source writes a shifted destination, as in Doom) rather than
// gather: with a gather, neighbors copy the same source and merge into
// horizontal smears; with a scatter, lineages split and rise as strands.
//
// Two passes per row, so the result does not depend on the loop direction.
// First, heat that rises straight lands in place and is marked; a cell whose
// heat drifts away keeps its old value but cooled (the classic version leaves
// it exactly as it was, and at the upwind edge under wind such cells never
// cool and form a full-heat column). Then drifting heat lands, but only on
// cells that got no straight-up heat. Hot cores therefore win, and rise as
// unbroken strands with cooler, drifting edges: the look of flame tongues.
// (Letting drift win instead turns the strands back into speckle, and a single
// left-to-right pass lets heat drifting left win and heat drifting right lose,
// so wind would cool one way and heat the other.)
void fire_step(Fire *fire) {
  const uint16_t w = fire->w;
  const uint16_t stride = fire->stride;
  const int cool_quantum = fire->cool_quantum;
  const uint32_t cool_chance = fire->cool_chance;
  const int8_t *shifts = fire->shifts;
  uint32_t *drift = fire->drift;
  uint8_t *straight = fire->straight;
  const uint16_t mask_bytes = (stride + 7) / 8;
  uint32_t r = fire->rng;
  for (uint16_t y = 1; y <= fire->h; y++) {
    const uint8_t *src = cell_row(fire, y);
    uint8_t *dst = (uint8_t *)src - stride;
    uint16_t n = 0;
    memset(straight, 0, mask_bytes);
    for (uint16_t x = 0; x < w; x++) {
      r ^= r << 13;
      r ^= r >> 17;
      r ^= r << 5;
      // High bits are the better-mixed ones: 16-17 shift, 20-29 cooling.
      // Only heat that cools drifts sideways, wind included, while the hot
      // cores rise straight. This coupling, after
      // filipedeschamps/doom-fire-algorithm where one random number is both
      // the decay and the offset, turns uncorrelated speckle into rising
      // strands and tongues, and makes wind lean the flames instead of
      // blowing the whole fire off the upwind edge.
      if (((r >> 20) & 1023) < cool_chance) {
        int heat = src[x] - cool_quantum;
        int to = x + shifts[(r >> 16) & 3];  // within the row's padding
        if (to == x) {
          dst[x] = heat > 0 ? heat : 0;
          straight[(x + FIRE_MAX_SHIFT) >> 3] |= 1 << ((x + FIRE_MAX_SHIFT) & 7);
        } else {
          drift[n++] = (uint32_t)(to + FIRE_MAX_SHIFT) << 8 | (heat > 0 ? heat : 0);
          // The heat left this cell. Unless drift lands here, it keeps its
          // old value, as in the classic version, but cooled like any other.
          int old = dst[x] - cool_quantum;
          dst[x] = old > 0 ? old : 0;
        }
      } else {
        dst[x] = src[x];
        straight[(x + FIRE_MAX_SHIFT) >> 3] |= 1 << ((x + FIRE_MAX_SHIFT) & 7);
      }
    }
    uint8_t *base = dst - FIRE_MAX_SHIFT;
    for (uint16_t i = 0; i < n; i++) {
      uint32_t at = drift[i] >> 8;
      if (!(straight[at >> 3] & (1 << (at & 7)))) {
        base[at] = drift[i] & 0xFF;
      }
    }
  }
  fire->rng = r;
  calibrate(fire);
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
  int spread = 0;
  for (int i = 0; i < 4; i++) {
    int s = shifts[i];
    fire->turbulence[i] = s > 2 ? 2 : (s < -2 ? -2 : s);
    spread += fire->turbulence[i] < 0 ? -fire->turbulence[i] : fire->turbulence[i];
  }
  // Start calibration near where it settles (host measurements: 1.0 for Calm,
  // 0.74-0.97 for Normal, 0.66-0.92 for Wild) so the flames start out close
  // to the requested height.
  fire->gain = GAIN_ONE * 12 / (12 + spread);
  fire->frames = 0;
  fire->top_sum = 0;
  update_shifts(fire);
  update_cooling(fire);
}

void fire_stoke(Fire *fire, uint8_t heat) {
  for (uint16_t y = 0; y < fire->h; y++) {
    uint8_t *row = cell_row(fire, y);
    for (uint16_t x = 0; x < fire->w; x++) {
      int v = row[x] + heat;
      row[x] = v > FIRE_MAX_HEAT ? FIRE_MAX_HEAT : v;
    }
  }
  // The extra heat takes about one transport time to rise out of the top;
  // keep it out of the calibration, which would otherwise over-cool.
  fire->hold = fire->h + 8;
}

void fire_set_fuel(Fire *fire, uint8_t fuel) {
  fire->fuel = fuel > FIRE_MAX_HEAT ? FIRE_MAX_HEAT : fuel;
  update_cooling(fire);
}
