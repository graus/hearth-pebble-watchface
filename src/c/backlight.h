#pragma once
#include <pebble.h>
#include "fire.h"

// Flame-colored, flickering backlight. Compiled only where the backlight is RGB.
#if defined(PBL_RGB_BACKLIGHT)

// Keeps the light on and colors it from the current flames.
// flicker: 0..100 % of the full random jitter.
void backlight_update(const Fire *fire, uint8_t flicker);

// Hands the light back to the system (timeout, ambient sensor, user color).
void backlight_release(void);

#endif
