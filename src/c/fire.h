#pragma once
// The PSX Doom fire (https://fabiensanglard.net/doom_fire_psx/), ported
// faithfully. Plain C with no Pebble dependency so it can be tested on the
// host.
//
// The propagation is Doom's spreadFire, unchanged. The watch face adapts
// only what is around it:
//   - Flame height: Doom's flames lose about half a heat level per row. The
//     engine runs as many rows as the requested tip heat needs (from a table
//     measured per wind), and the renderer stretches them over the digit
//     window.
//   - Wind: the "+ 1" in Doom's `dst = src - randIdx + 1` sets the drift
//     (Doom's own fire leans left half a cell per row); wind changes that
//     constant.
//   - Edges wrap around within the row; Doom's flat array wraps into the
//     neighboring row instead, which has no meaning on a watch.

#include <stdint.h>

// Heat levels run 0..FIRE_MAX_HEAT; the palette has one entry per level.
#define FIRE_MAX_HEAT 36
#define FIRE_PALETTE_SIZE (FIRE_MAX_HEAT + 1)

// Highest tip heat the reach accepts, and the most rows that can need.
#define FIRE_MAX_REACH 20
#define FIRE_MAX_ROWS 96

typedef struct Fire Fire;

// w columns; the rows follow the reach (tip heat 9 to start). The grid holds
// one extra row below them: the fuel, always FIRE_MAX_HEAT, never drawn.
// Returns NULL on out-of-memory.
Fire *fire_create(uint16_t w, uint32_t seed);
void fire_destroy(Fire *fire);

// Advance one frame: Doom's spreadFire for every pixel.
void fire_step(Fire *fire);

// Visible row y (0 = top), w cells long.
const uint8_t *fire_row(const Fire *fire, uint16_t y);
uint16_t fire_width(const Fire *fire);
uint16_t fire_height(const Fire *fire);

// Reach: the mean heat at the top row (the flame tips), 0..FIRE_MAX_REACH.
// Sets the number of rows (as does the wind).
void fire_set_reach(Fire *fire, uint8_t top_heat);
// Wind: -2..3, added to Doom's "+ 1". 0 is Doom exactly (a slight lean to
// the left); 1 leans as much to the right.
void fire_set_wind(Fire *fire, int8_t wind);

// Stoke: add heat to every cell at once (clamped), for an immediate leap
// that then burns down on its own. An outside event like Doom's own
// "fire on/off"; the propagation is untouched.
void fire_stoke(Fire *fire, uint8_t heat);
