#pragma once
// Doom-style fire simulation (https://fabiensanglard.net/doom_fire_psx/).
// Plain C with no Pebble dependency so it can be tested on the host.

#include <stdint.h>

// Heat levels run 0..FIRE_MAX_HEAT; the palette has one entry per level.
#define FIRE_MAX_HEAT 36
#define FIRE_PALETTE_SIZE (FIRE_MAX_HEAT + 1)

// Largest sideways move in one step: turbulence (2) plus wind (2). Each row
// carries this many padding cells per side, so a shifted read never leaves
// the grid whatever the settings.
#define FIRE_MAX_SHIFT 4

typedef struct Fire Fire;

// w x h visible cells. The grid holds one extra row below them: the permanent
// fuel row, always FIRE_MAX_HEAT, never drawn. Returns NULL on out-of-memory.
Fire *fire_create(uint16_t w, uint16_t h, uint32_t seed);
void fire_destroy(Fire *fire);

// Advance one frame.
void fire_step(Fire *fire);

// Visible row y (0 = top), w cells long.
const uint8_t *fire_row(const Fire *fire, uint16_t y);
uint16_t fire_width(const Fire *fire);
uint16_t fire_height(const Fire *fire);

// Reach: the mean heat that arrives at the top row, 0..FIRE_MAX_HEAT.
void fire_set_reach(Fire *fire, uint8_t top_heat);
// Wind: sideways drift of cooling heat, -2..2 (positive blows right).
void fire_set_wind(Fire *fire, int8_t wind);
// Turbulence: the four sideways shifts picked at random per cell, each -2..2.
void fire_set_turbulence(Fire *fire, const int8_t shifts[4]);
// Fuel: extra reach added on top of the reach setting (flare), 0..FIRE_MAX_HEAT.
// Works through the cooling, so it shows at the top only after a while.
void fire_set_fuel(Fire *fire, uint8_t fuel);

// Stoke: add heat to every cell at once (clamped), for an immediate leap that
// then cools back on its own.
void fire_stoke(Fire *fire, uint8_t heat);
