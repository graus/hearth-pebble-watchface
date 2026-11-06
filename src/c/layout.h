#pragma once
#include <pebble.h>

typedef struct {
  GRect digits[4];  // HH over MM: 0, 1 on the top row, 2, 3 below
  GRect window;     // flame window: the digit block's bounding box
} Layout;

// Places the four digits (given their bitmap sizes) in the unobstructed area.
// Rows are centered by their real width; the block is centered vertically.
// The flame window is derived from the worst-case metrics so it does not
// change from minute to minute (only when the unobstructed area changes).
void layout_compute(Layout *out, GRect area, int screen_w, const GSize digit_sizes[4]);
