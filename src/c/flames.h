#pragma once
#include <pebble.h>
#include "fire.h"
#include "settings.h"

// Turns white digit pixels in the framebuffer into flame pixels.
//   Color: one palette lookup per digit pixel, cells of (1 << shift) px.
//   B&W: 4 px cells drawn with 4x4 ordered dither patterns.

// Builds the palette (color) or dither table (B&W) for the settings.
void flames_configure(const Settings *s);

// window: flame window in screen coordinates (byte-aligned on B&W).
void flames_render(GBitmap *fb, const Fire *fire, GRect window, uint8_t shift);
